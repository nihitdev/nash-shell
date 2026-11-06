#include "builtins.h"
#include "config.h"
#include "editor.h"
#include "executor.h"
#include "integrations.h"
#include "prompt.h"
#include "shell.h"
#include "util.h"
#include <cerrno>
#include <chrono>
#include <clocale>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <unistd.h>
int main(int argc, char **argv) {
    std::setlocale(LC_ALL, "");
    ang::ShellState st;
    bool no_config = false, has_command = false;
    std::string command, script;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--posix")
            st.posix = true;
        else if (arg == "--ang")
            st.posix = false;
        else if (arg == "--no-config")
            no_config = true;
        else if (arg == "-c" || arg == "--command") {
            if (++i == argc) {
                std::cerr << "nash: -c requires a command\n";
                return 2;
            }
            command = argv[i];
            has_command = true;
            break;
        } else if (arg == "--version") {
            std::cout << "Nash Shell " NASH_VERSION "\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: nash [--no-config] [--posix|--ang] [-c command] [script]\n";
            return 0;
        } else if (arg.starts_with('-')) {
            std::cerr << "nash: unknown option " << arg << '\n';
            return 2;
        } else {
            script = arg;
            break;
        }
    }
    st.interactive = isatty(0) && isatty(1) && !has_command && script.empty();
    ang::initialize_terminal(st);
    ang::apply_defaults(st);
    if (st.interactive && !no_config) {
        ang::load_config(st);
        if (!ang::g_should_exit)
            ang::run_startup_tools(st);
    }
    if (const char *p = getenv("NASH_POSIX"); p && std::string(p) == "1")
        st.posix = true;
    if (st.interactive) {
        setenv("STARSHIP_SHELL", "nash", 1);
        setenv("STARSHIP_SESSION_KEY", std::to_string(getpid()).c_str(), 1);
    }
    if (has_command)
        return ang::run_source(command, st);
    if (!st.interactive) {
        if (!script.empty()) {
            std::ifstream file(script);
            if (!file) {
                std::cerr << "nash: cannot read " << script << '\n';
                return 127;
            }
            std::ostringstream source;
            source << file.rdbuf();
            ang::run_source(source.str(), st);
        } else {
            // Consume one logical command at a time; builtin read shares fd 0.
            std::string pending;
            while (!ang::g_should_exit) {
                char c;
                const auto count = ::read(STDIN_FILENO, &c, 1);
                if (count < 0) {
                    if (errno == EINTR)
                        continue;
                    std::cerr << "nash: stdin: read failed\n";
                    st.last_status = 1;
                    break;
                }
                if (count == 0) {
                    if (!pending.empty())
                        ang::run_source(pending, st);
                    break;
                }
                if (c == '\n' && ang::input_complete(pending)) {
                    ang::run_source(pending, st);
                    pending.clear();
                } else
                    pending += c;
            }
        }
        return ang::g_should_exit ? ang::g_exit_code : st.last_status;
    }
    std::string pending;
    while (!ang::g_should_exit) {
        ang::reap_jobs(st);
        ang::run_hook("precmd", st);
        if (ang::g_should_exit)
            break;
        auto left = pending.empty() ? ang::make_prompt(st, false) : "... ";
        auto right = pending.empty() ? ang::make_prompt(st, true) : "";
        std::string line;
        if (!ang::read_line(st, left, right, &line)) {
            if (!pending.empty()) {
                std::cerr << "nash: unexpected end of input\n";
                st.last_status = 2;
            }
            break;
        }
        if (st.input_cancelled) {
            st.input_cancelled = false;
            pending.clear();
            continue;
        }
        pending += line;
        if (!ang::input_complete(pending)) {
            pending += '\n';
            continue;
        }
        if (ang::trim(pending).empty()) {
            pending.clear();
            continue;
        }
        ang::run_hook("preexec", st);
        auto before = std::chrono::steady_clock::now();
        ang::run_source(pending, st);
        st.command_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - before)
                                     .count();
        // Record after evaluation so !! refers to the preceding command.
        if (st.history_enabled && pending[0] != ' ' &&
            (st.history.empty() || st.history.back() != pending))
            st.history.push_back(pending);
        pending.clear();
        ang::save_history(st);
    }
    ang::save_history(st);
    for (const auto &job : st.bg_jobs)
        if (!job.reaped) {
            kill(-job.pid, SIGHUP);
            if (job.stopped)
                kill(-job.pid, SIGCONT);
        }
    tcsetpgrp(0, st.shell_pgid);
    tcsetattr(0, TCSADRAIN, &st.terminal_modes);
    return ang::g_should_exit ? ang::g_exit_code : st.last_status;
}
