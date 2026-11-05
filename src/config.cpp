// config.cpp
#include "config.h"
#include "builtins.h"
#include "executor.h"
#include "lexer.h"
#include "parser.h"
#include "prompt.h"
#include "util.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace ang {

void apply_defaults(ShellState& st) {
    st.prompt_left = "{gray}{user}@{host}{reset} {bold}{blue}{cwd}{reset}{purple}{branch}{reset} {arrow} ";
    st.prompt_right = "{dim}{mode}{time}{reset}";
    st.history_file = data_dir() + "/history";
}

static void run_quiet(const std::string& line, ShellState& st) {
    bool skip = false;
    // Never let user config see interactive-only expansions as fatal.
    bool was = st.interactive;
    st.interactive = false;
    std::string s = preprocess(line, st, &skip);
    st.interactive = was;
    if (skip) return;
    auto toks = lex(s, nullptr);
    if (toks.empty()) return;
    // Autocd / partial lines in config: ignore errors silently.
    if (toks.size() == 1 && toks[0].kind == TokKind::Word) {
        std::string w = expand_tilde(toks[0].text);
        std::error_code ec;
        if (std::filesystem::is_directory(w, ec)) return;
    }
    JobList jobs;
    std::string err;
    if (!parse(toks, st.posix, &jobs, &err)) return;
    run_jobs(jobs, st);
}

void load_config(ShellState& st) {
    apply_defaults(st);

    // Theme first (config can override with `theme x`).
    std::string err;
    load_theme("default", st, nullptr);
    std::ifstream tc(config_dir() + "/theme.conf");
    if (tc) load_theme(config_dir() + "/theme.conf", st, nullptr);
    (void)err;

    // Main config: plain nash commands.
    std::ifstream cfg(config_dir() + "/config.ang");
    std::string line;
    if (cfg) {
        while (std::getline(cfg, line)) {
            run_quiet(line, st);
            if (g_should_exit) { g_should_exit = false; break; }
        }
    }
    // Plugins: each *.ang sourced in sorted order.
    std::error_code ec;
    std::string plugdir = config_dir() + "/plugins";
    if (std::filesystem::is_directory(plugdir, ec)) {
        std::vector<std::string> files;
        for (auto& e : std::filesystem::directory_iterator(plugdir, ec)) {
            if (e.path().extension() == ".ang") files.push_back(e.path().string());
        }
        std::sort(files.begin(), files.end());
        for (auto& f : files) {
            std::vector<std::string> args = {"source", f};
            bi_source(args, st);
            if (g_should_exit) { g_should_exit = false; break; }
        }
    }

    // Env override wins for scripting: NASH_POSIX=1 nash
    if (const char* p = getenv("NASH_POSIX"))
        if (std::string(p) == "1" || std::string(p) == "on") st.posix = true;

    // Load history
    std::ifstream h(st.history_file);
    if (h) {
        while (std::getline(h, line)) {
            if (!line.empty()) st.history.push_back(line);
        }
        if (st.history.size() > 10000)
            st.history.erase(st.history.begin(),
                             st.history.end() - 10000);
    }
}

}  // namespace ang
