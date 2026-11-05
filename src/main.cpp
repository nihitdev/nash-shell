// main.cpp — nash entry point: CLI flags, script mode, and the REPL.
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <cstdio>
#include <signal.h>
#include <sys/wait.h>
#include "builtins.h"
#include "config.h"
#include "editor.h"
#include "executor.h"
#include "lexer.h"
#include "parser.h"
#include "prompt.h"
#include "util.h"

#ifndef NASH_VERSION
#define NASH_VERSION "0.1.3"
#endif

namespace {
void usage() {
    std::cout <<
        "usage: nash [--posix|--ang] [--no-config] [-c CMD] [SCRIPT [ARGS...]]\n"
        "  --posix      start in POSIX-compatible mode\n"
        "  --ang        start in Ang mode (default)\n"
        "  -c CMD       run CMD and exit\n"
        "  SCRIPT       run file as nash script\n"
        "  --version    print version\n"
        "  --help       print this help\n";
}

int run_chunk(const std::string& raw, ang::ShellState& st);

// SIGCHLD handler: just flip a flag; reaping happens in the main loop
// (async-signal-safe). waitpid in a handler is not safe.
static volatile sig_atomic_t g_bg_tick = 0;
static void on_sigchld(int /*sig*/) { g_bg_tick = 1; }

int run_one(const std::string& raw, ang::ShellState& st) {
    // Process line-by-line (like bash reading script lines): this lets
    // `alias`/`posix on` on one line affect the following lines.
    size_t pos = 0;
    while (pos <= raw.size()) {
        size_t nl = raw.find('\n', pos);
        std::string chunk = raw.substr(pos, nl == std::string::npos ? nl : nl - pos);
        if (nl == std::string::npos) pos = raw.size() + 1;
        else pos = nl + 1;
        run_chunk(chunk, st);
        if (ang::g_should_exit) break;
    }
    return st.last_status;
}

// Single physical line: expand, lex, parse, run.
int run_chunk(const std::string& raw, ang::ShellState& st) {
    bool skip = false;
    std::string s = ang::preprocess(raw, st, &skip);
    if (skip) return st.last_status;
    if (ang::trim(s).empty()) return st.last_status;

    // zsh-style autocd: bare directory name cds into it.
    {
        auto toks = ang::lex(s, nullptr);
        if (st.autocd && toks.size() == 1 &&
            toks[0].kind == ang::TokKind::Word) {
            std::string w = ang::expand_tilde(toks[0].text);
            std::error_code ec;
            if (std::filesystem::is_directory(w, ec)) {
                std::vector<std::string> a = {"cd", w};
                int rc = ang::bi_cd(a, st);
                st.last_status = rc;
                return rc;
            }
        }
    }

    std::string lerr;
    auto toks = ang::lex(s, &lerr);
    if (!lerr.empty()) {
        std::cerr << ang::err_prefix() << lerr << "\n";
        st.last_status = 2;
        return 2;
    }
    if (toks.empty()) return st.last_status;
    ang::JobList jobs;
    std::string perr;
    if (!ang::parse(toks, st.posix, &jobs, &perr)) {
        std::cerr << ang::err_prefix() << perr << "\n";
        st.last_status = 2;
        return 2;
    }
    return ang::run_jobs(jobs, st);
}
}  // namespace

int main(int argc, char** argv) {
    ang::ShellState st;
    bool no_config = false;
    std::string cmd;
    std::string script;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--posix" || a == "-o-posix") st.posix = true;
        else if (a == "--ang") st.posix = false;
        else if (a == "--no-config" || a == "--noconfig") no_config = true;
        else if ((a == "-c" || a == "--command") && i + 1 < argc) cmd = argv[++i];
        else if (a == "--version" || a == "-v") {
            std::cout << "nash " NASH_VERSION "\n";
            return 0;
        } else if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (!a.empty() && a[0] == '-') { usage(); return 2; }
        else { script = a; break; }
    }

    // Parent ignores Ctrl-C (foreground children restore default, see executor).
    // Background jobs report completion via SIGCHLD -> g_bg_tick -> reap below.
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, on_sigchld);

    st.interactive = isatty(STDIN_FILENO) && script.empty() && cmd.empty();
    if (!no_config) ang::load_config(st);
    else ang::apply_defaults(st);
    // Env override wins for scripting in every mode: NASH_POSIX=1 nash
    if (const char* p = getenv("NASH_POSIX"))
        if (std::string(p) == "1" || std::string(p) == "on") st.posix = true;

    if (!cmd.empty()) return run_one(cmd, st);

    if (!script.empty()) {
        std::vector<std::string> a = {"source", script};
        bool h = false;
        int rc = ang::run_builtin(a, st, &h);
        return h ? rc : 0;
    }

    st.interactive = isatty(STDIN_FILENO);
    if (st.interactive) {
        bool c = ang::use_color(STDOUT_FILENO);
        std::string B = c ? "\033[1;36m" : "";
        std::string D = c ? "\033[2m" : "";
        std::string R = c ? "\033[0m" : "";
        std::cout << B
                  << "   __ _ _ __   __ _ ___| |__\n"
                  << "  / _` | '_ \\ / _` / __| '_ \\\n"
                  << " | (_| | | | | (_| \\__ \\ | | |\n"
                  << "  \\__,_|_| |_|\\__, |___/_| |_|\n"
                  << "               |___/  " << R
                  << D << "v" NASH_VERSION
                  << (st.posix ? " · posix mode" : " · ang mode")
                  << " · theme " << st.theme_name << R << "\n"
                  << D << "  type `help` to see what nash can do." << R << "\n";
    }

    while (!ang::g_should_exit) {
        // Reap finished background jobs and report them before drawing the prompt.
        if (g_bg_tick) {
            g_bg_tick = 0;
            bool any = false;
            for (auto& j : st.bg_jobs) {
                if (j.reaped) continue;
                int ws = 0;
                if (waitpid(j.pid, &ws, WNOHANG) == 0) continue;  // still running
                j.reaped = true;
                j.status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 128 + WTERMSIG(ws);
                std::fprintf(stderr, "\r[done] %d (%d) %s\n", (int)j.pid, j.status,
                             j.cmd.c_str());
                any = true;
            }
            if (any) std::fflush(stderr);
        }
        // Piped stdin: no prompts, run like a script.
        std::string left = st.interactive ? ang::render_prompt(st.prompt_left, st) : "";
        std::string right = st.interactive ? ang::render_prompt(st.prompt_right, st) : "";
        std::string line;
        if (!ang::read_line(st, left, right, &line)) break;  // EOF
        if (ang::trim(line).empty()) continue;
        // history: skip consecutive duplicates (fish/bash behavior)
        if (st.history.empty() || st.history.back() != line)
            st.history.push_back(line);
        run_one(line, st);
    }

    ang::save_history(st);
    return ang::g_should_exit ? ang::g_exit_code : st.last_status;
}
