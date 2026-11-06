// builtins.cpp
#include "builtins.h"
#include "config.h"
#include "executor.h"
#include "integrations.h"
#include "lexer.h"
#include "parser.h"
#include "prompt.h"
#include "shell.h"
#include "util.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <signal.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

#ifndef NASH_VERSION
#define NASH_VERSION "0.2.0"
#endif

namespace ang {

static int need_args(const std::vector<std::string> &a, size_t n, const std::string &use) {
    if (a.size() < n) {
        std::cerr << "usage: " << use << "\n";
        return 2;
    }
    return -1;
}

bool is_parent_builtin(const std::string &name) {
    return name == "cd" || name == "exit" || name == "logout" || name == "export" ||
           name == "unset" || name == "set" || name == "alias" || name == "unalias" ||
           name == "abbr" || name == "posix" || name == "source" || name == "." ||
           name == "theme" || name == "autocd" || name == "history" || name == "read" ||
           name == "jobs" || name == "wait" || name == "fg" || name == "bg" ||
           name == "nash-prompt" || name == "integrations" || name == "config" || name == "has";
}

int run_builtin(const std::vector<std::string> &args, ShellState &st, bool *handled) {
    *handled = true;
    const std::string &c = args[0];
    if (c == "config")
        return bi_config(args, st);
    if (c == "has") {
        if (args.size() < 2)
            return 2;
        for (size_t i = 1; i < args.size(); ++i)
            if (!find_executable(args[i]))
                return 1;
        return 0;
    }
    if (c == "cd")
        return bi_cd(args, st);
    if (c == "exit" || c == "logout")
        return bi_exit(args, st);
    if (c == "echo")
        return bi_echo(args, st);
    if (c == "pwd")
        return bi_pwd(args, st);
    if (c == "export")
        return bi_export(args, st);
    if (c == "unset")
        return bi_unset(args, st);
    if (c == "set")
        return bi_set(args, st);
    if (c == "alias")
        return bi_alias(args, st);
    if (c == "unalias")
        return bi_unalias(args, st);
    if (c == "abbr")
        return bi_abbr(args, st);
    if (c == "posix")
        return bi_posix(args, st);
    if (c == "source" || c == ".")
        return bi_source(args, st);
    if (c == "history")
        return bi_history(args, st);
    if (c == "help")
        return bi_help(args, st);
    if (c == "version" || c == "--version")
        return bi_version(args, st);
    if (c == "true" || c == ":")
        return bi_true(args, st);
    if (c == "false")
        return bi_false(args, st);
    if (c == "type" || c == "which")
        return bi_type(args, st);
    if (c == "theme")
        return bi_theme(args, st);
    if (c == "env" && args.size() == 1)
        return bi_env(args, st);
    if (c == "printenv") {
        if (args.size() == 1)
            return bi_env(args, st);
        int result = 0;
        for (size_t i = 1; i < args.size(); ++i) {
            const char *value = getenv(args[i].c_str());
            if (value)
                std::cout << value << '\n';
            else
                result = 1;
        }
        return result;
    }
    if (c == "read")
        return bi_read(args, st);
    if (c == "integrations")
        return bi_integrations(args, st);
    if (c == "nash-prompt") {
        auto preview = st;
        bool right = false;
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--right")
                right = true;
            else if (args[i] == "--engine" && i + 1 < args.size()) {
                preview.prompt_engine = args[++i];
                if (preview.prompt_engine != "native" && preview.prompt_engine != "starship")
                    return 2;
            } else
                return 2;
        }
        std::cout << make_prompt(preview, right);
        return 0;
    }
    if (c == "fg" || c == "bg")
        return bi_job_control(args, st);
    if (c == "jobs")
        return bi_jobs(args, st);
    if (c == "wait")
        return bi_wait(args, st);
    if (c == "autocd") {
        if (args.size() > 1)
            st.autocd = (args[1] == "on" || args[1] == "1");
        std::cout << "autocd " << (st.autocd ? "on" : "off") << "\n";
        return 0;
    }
    *handled = false;
    return 0;
}

int bi_cd(const std::vector<std::string> &a, ShellState &st) {
    size_t first = 1;
    if (first < a.size() && a[first] == "--")
        ++first;
    if (a.size() > first + 1) {
        std::cerr << "nash: cd: too many arguments\n";
        return 2;
    }
    const bool previous = first < a.size() && a[first] == "-";
    std::string destination = first < a.size() ? expand_tilde(a[first]) : home_dir();
    if (previous) {
        destination = st.get("OLDPWD");
        if (destination.empty()) {
            std::cerr << "nash: cd: OLDPWD not set\n";
            return 1;
        }
    }
    char old[PATH_MAX];
    const std::string old_path = getcwd(old, sizeof(old)) ? old : st.get("PWD");
    if (chdir(destination.c_str()) < 0) {
        std::cerr << "nash: cd: " << destination << ": " << strerror(errno) << '\n';
        return 1;
    }
    char current[PATH_MAX];
    if (getcwd(current, sizeof(current)))
        st.set_var("PWD", current, true);
    st.set_var("OLDPWD", old_path, true);
    if (previous)
        std::cout << st.get("PWD") << '\n';
    run_hook("chpwd", st);
    return 0;
}
int bi_exit(const std::vector<std::string> &a, ShellState &st) {
    long long code = st.last_status;
    if (a.size() > 1) {
        auto value = trim(a[1]);
        char *end = nullptr;
        errno = 0;
        code = strtoll(value.c_str(), &end, 10);
        if (value.empty() || end != value.c_str() + value.size() || errno == ERANGE) {
            std::cerr << "nash: exit: " << a[1] << ": numeric argument required\n";
            g_should_exit = true;
            return g_exit_code = 2;
        }
        if (a.size() > 2) {
            std::cerr << "nash: exit: too many arguments\n";
            if (!st.interactive) {
                g_should_exit = true;
                g_exit_code = 1;
            }
            return 1;
        }
    }
    g_should_exit = true;
    return g_exit_code = static_cast<unsigned char>(code);
}

int bi_echo(const std::vector<std::string> &a, ShellState &st) {
    (void)st;
    bool nl = true, esc = false;
    size_t i = 1;
    // leading flags (-n, -e, -en, -ne) like bash
    for (; i < a.size(); ++i) {
        const std::string &f = a[i];
        if (f == "-n")
            nl = false;
        else if (f == "-e")
            esc = true;
        else if (f == "-en" || f == "-ne") {
            nl = false;
            esc = true;
        } else
            break;
    }
    bool first = true;
    for (; i < a.size(); ++i) {
        if (!first)
            std::cout << " ";
        first = false;
        if (!esc) {
            std::cout << a[i];
            continue;
        }
        for (size_t j = 0; j < a[i].size(); ++j) {
            char c = a[i][j];
            if (c == '\\' && j + 1 < a[i].size()) {
                char d = a[i][j + 1];
                if (d == 'n')
                    std::cout << '\n';
                else if (d == 't')
                    std::cout << '\t';
                else if (d == '\\')
                    std::cout << '\\';
                else
                    std::cout << '\\' << d;
                ++j;
            } else {
                std::cout << c;
            }
        }
    }
    if (nl)
        std::cout << "\n";
    std::cout.flush();
    return 0;
}

int bi_pwd(const std::vector<std::string> &, ShellState &) {
    char b[PATH_MAX];
    if (getcwd(b, sizeof b)) {
        std::cout << b << "\n";
        return 0;
    }
    perror("pwd");
    return 1;
}

// export NAME=value / export NAME  (POSIX style)
int bi_export(const std::vector<std::string> &a, ShellState &st) {
    if (a.size() == 1) {
        for (auto &kv : st.vars)
            std::cout << "export " << kv.first << "=\"" << kv.second << "\"\n";
        return 0;
    }
    for (size_t i = 1; i < a.size(); ++i) {
        std::string n, v;
        if (is_assign_word(a[i], &n, &v))
            st.set_var(n, v, true);
        else if (is_valid_name(a[i])) {
            // export NAME: promote an environ-only variable too, keeping
            // its value instead of clobbering it to empty.
            std::string val;
            auto it = st.vars.find(a[i]);
            if (it != st.vars.end())
                val = it->second;
            else if (const char *e = std::getenv(a[i].c_str()))
                val = e;
            st.set_var(a[i], val, true);
        } else {
            std::cerr << "export: invalid name: " << a[i] << "\n";
            return 1;
        }
    }
    return 0;
}

int bi_unset(const std::vector<std::string> &a, ShellState &st) {
    if (need_args(a, 2, "unset NAME...") == 2)
        return 2;
    for (size_t i = 1; i < a.size(); ++i)
        st.unset_var(a[i]);
    return 0;
}

// set NAME value...  (Ang style, fish-like). `set --posix on|off` toggles mode.
int bi_set(const std::vector<std::string> &a, ShellState &st) {
    if (a.size() >= 2 && a[1] == "--posix") {
        if (a.size() < 3) {
            std::cout << (st.posix ? "on" : "off") << "\n";
            return 0;
        }
        st.posix = (a[2] == "on" || a[2] == "1" || a[2] == "true");
        return 0;
    }
    if (need_args(a, 3, "set NAME value...  (ang syntax; POSIX: export NAME=value)") == 2)
        return 2;
    if (!is_valid_name(a[1])) {
        std::cerr << "set: invalid name: " << a[1] << "\n";
        return 1;
    }
    // `set NAME` with no value errors; `set NAME ""` sets an empty value
    std::string v = join(std::vector<std::string>(a.begin() + 2, a.end()), " ");
    st.set_var(a[1], v, true);
    return 0;
}

int bi_alias(const std::vector<std::string> &a, ShellState &st) {
    if (a.size() == 1) {
        for (auto &kv : st.aliases)
            std::cout << "alias " << kv.first << "='" << kv.second << "'\n";
        return 0;
    }
    // fish-style: alias NAME body...  (e.g. alias ll 'ls -l')
    if (a.size() >= 3 && a[1].find('=') == std::string::npos) {
        st.aliases[a[1]] = join(std::vector<std::string>(a.begin() + 2, a.end()), " ");
        return 0;
    }
    for (size_t i = 1; i < a.size(); ++i) {
        size_t eq = a[i].find('=');
        if (eq == std::string::npos) {
            auto it = st.aliases.find(a[i]);
            if (it != st.aliases.end())
                std::cout << "alias " << it->first << "='" << it->second << "'\n";
            else {
                std::cerr << "alias: not found: " << a[i] << "\n";
                return 1;
            }
        } else {
            std::string n = a[i].substr(0, eq), v = a[i].substr(eq + 1);
            if (v.size() >= 2 &&
                ((v.front() == '\'' && v.back() == '\'') || (v.front() == '"' && v.back() == '"')))
                v = v.substr(1, v.size() - 2);
            st.aliases[n] = v;
        }
    }
    return 0;
}

int bi_unalias(const std::vector<std::string> &a, ShellState &st) {
    if (need_args(a, 2, "unalias NAME...") == 2)
        return 2;
    for (size_t i = 1; i < a.size(); ++i)
        st.aliases.erase(a[i]);
    return 0;
}

// abbr add NAME EXPANSION | abbr list | abbr rm NAME  (fish-like)
int bi_abbr(const std::vector<std::string> &a, ShellState &st) {
    if (a.size() == 1 || a[1] == "list") {
        for (auto &kv : st.abbrs)
            std::cout << kv.first << " -> " << kv.second << "\n";
        return 0;
    }
    if (a[1] == "add" && a.size() >= 4) {
        st.abbrs[a[2]] = join(std::vector<std::string>(a.begin() + 3, a.end()), " ");
        return 0;
    }
    if ((a[1] == "rm" || a[1] == "erase") && a.size() == 3) {
        st.abbrs.erase(a[2]);
        return 0;
    }
    std::cerr << "usage: abbr add NAME EXPANSION | abbr rm NAME | abbr list\n";
    return 2;
}

int bi_posix(const std::vector<std::string> &a, ShellState &st) {
    if (a.size() == 1) {
        std::cout << (st.posix ? "on" : "off") << "\n";
        return 0;
    }
    if (a[1] == "on" || a[1] == "off" || a[1] == "status") {
        if (a[1] != "status")
            st.posix = (a[1] == "on");
        std::cout << "posix " << (st.posix ? "on" : "off") << "\n";
        return 0;
    }
    std::cerr << "usage: posix [on|off|status]\n";
    return 2;
}

int bi_source(const std::vector<std::string> &a, ShellState &st) {
    if (a.size() != 2) {
        std::cerr << "source: expected FILE\n";
        return 2;
    }
    std::ifstream in(expand_tilde(a[1]));
    if (!in) {
        std::cerr << "source: cannot open " << a[1] << '\n';
        return 1;
    }
    static int depth = 0;
    if (depth >= 32) {
        std::cerr << "source: recursion limit\n";
        return 2;
    }
    ++depth;
    std::ostringstream text;
    text << in.rdbuf();
    int rc = run_source(text.str(), st);
    --depth;
    return rc;
}

int bi_history(const std::vector<std::string> &a, ShellState &st) {
    if (a.size() > 1 && a[1] == "clear") {
        st.history.clear();
        return 0;
    }
    for (size_t i = 0; i < st.history.size(); ++i)
        std::cout << i + 1 << "  " << st.history[i] << "\n";
    return 0;
}

int bi_help(const std::vector<std::string> &, ShellState &) {
    std::cout << "Nash Shell " NASH_VERSION " — fork of ang.sh\n"
              << "Builtins: cd pwd exit echo export unset set alias unalias abbr posix\n"
              << "          source history help version true false type theme env printenv\n"
              << "          read jobs fg bg wait integrations nash-prompt config has\n"
              << "Syntax: pipelines, redirections, && || ; &, assignments, quoting, $(...)\n"
              << "Config: ~/.config/nash/config.nash; Starship uses existing starship.toml\n"
              << "Keys: Tab completion, Right/Ctrl-F suggestion, Ctrl-R history, Ctrl-A/E/U/K/W\n"
              << "Functions, loops, if/case, heredocs and full POSIX compatibility are not "
                 "implemented.\n";
    return 0;
}

int bi_version(const std::vector<std::string> &, ShellState &st) {
    std::cout << st.shell_name << " " NASH_VERSION << (st.posix ? " (posix)" : " (ang)") << "\n";
    return 0;
}
int bi_true(const std::vector<std::string> &, ShellState &) { return 0; }
int bi_false(const std::vector<std::string> &, ShellState &) { return 1; }

int bi_type(const std::vector<std::string> &a, ShellState &st) {
    size_t first = 1;
    if (first < a.size() && a[first] == "--")
        ++first;
    if (first == a.size()) {
        std::cerr << "usage: type NAME...\n";
        return 2;
    }
    int status = 0;
    for (size_t i = first; i < a.size(); ++i) {
        const auto &name = a[i];
        auto alias = st.aliases.find(name);
        auto abbreviation = st.abbrs.find(name);
        const bool builtin = is_parent_builtin(name) || name == "echo" || name == "pwd" ||
                             name == "true" || name == "false" || name == ":" || name == "type" ||
                             name == "which" || name == "help" || name == "version" ||
                             name == "env" || name == "printenv";
        if (alias != st.aliases.end())
            std::cout << name << " is an alias: " << alias->second << '\n';
        else if (abbreviation != st.abbrs.end())
            std::cout << name << " is an abbreviation: " << abbreviation->second << '\n';
        else if (builtin)
            std::cout << name << " is a shell builtin\n";
        else if (auto path = find_executable(name)) {
            if (a[0] == "which")
                std::cout << *path << '\n';
            else
                std::cout << name << " is " << *path << '\n';
        } else {
            std::cerr << "nash: " << name << ": not found\n";
            status = 1;
        }
    }
    return status;
}

int bi_env(const std::vector<std::string> &, ShellState &) {
    for (char **entry = ::environ; *entry; ++entry)
        std::cout << *entry << '\n';
    return 0;
}

namespace {
volatile sig_atomic_t read_interrupted = 0;
void interrupt_read(int) { read_interrupted = 1; }
struct ReadSignal {
    struct sigaction previous {};
    bool installed = false;
    explicit ReadSignal(bool interactive) {
        read_interrupted = 0;
        if (interactive) {
            struct sigaction action {};
            action.sa_handler = interrupt_read;
            sigemptyset(&action.sa_mask);
            installed = sigaction(SIGINT, &action, &previous) == 0;
        }
    }
    ~ReadSignal() {
        if (installed)
            sigaction(SIGINT, &previous, nullptr);
    }
};
} // namespace
// Read stdin directly so buffering/EOF from one redirected command cannot poison the next.
int bi_read(const std::vector<std::string> &a, ShellState &st) {
    size_t first = 1;
    bool raw = false;
    std::string prompt;
    while (first < a.size()) {
        if (a[first] == "--") {
            ++first;
            break;
        }
        if (a[first] == "-r") {
            raw = true;
            ++first;
            continue;
        }
        if (a[first] == "-p" && first + 1 < a.size()) {
            prompt = a[first + 1];
            first += 2;
            continue;
        }
        break;
    }
    if (first >= a.size()) {
        std::cerr << "usage: read [-r] [-p PROMPT] NAME...\n";
        return 2;
    }
    for (size_t i = first; i < a.size(); ++i)
        if (!is_valid_name(a[i])) {
            std::cerr << "nash: read: invalid variable name: " << a[i] << '\n';
            return 2;
        }
    ReadSignal signals(st.interactive);
    if (!prompt.empty()) {
        std::cerr << prompt;
        std::cerr.flush();
    }
    std::string line;
    std::vector<bool> protected_chars;
    bool newline = false, escape = false;
    while (!read_interrupted) {
        char c;
        const auto n = ::read(STDIN_FILENO, &c, 1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            std::cerr << "nash: read: " << strerror(errno) << '\n';
            return 1;
        }
        if (n == 0)
            break;
        if (escape) {
            escape = false;
            if (c == '\n')
                continue;
            line += c;
            protected_chars.push_back(true);
            continue;
        }
        if (!raw && c == '\\') {
            escape = true;
            continue;
        }
        if (c == '\n') {
            newline = true;
            break;
        }
        line += c;
        protected_chars.push_back(false);
    }
    if (read_interrupted) {
        if (st.interactive)
            std::cout << '\n';
        return 130;
    }
    const auto ifs = st.get("IFS", " \t\n");
    auto separator = [&](size_t p) {
        return p < line.size() && !protected_chars[p] && ifs.find(line[p]) != std::string::npos;
    };
    auto whitespace = [&](size_t p) {
        return separator(p) && (line[p] == ' ' || line[p] == '\t' || line[p] == '\n');
    };
    size_t pos = 0;
    for (size_t i = first; i < a.size(); ++i) {
        while (whitespace(pos))
            ++pos;
        const auto start = pos;
        if (i + 1 == a.size()) {
            size_t end = line.size();
            while (end > start && whitespace(end - 1))
                --end;
            st.set_var(a[i], line.substr(start, end - start), getenv(a[i].c_str()) != nullptr);
            pos = line.size();
        } else {
            while (pos < line.size() && !separator(pos))
                ++pos;
            st.set_var(a[i], line.substr(start, pos - start), getenv(a[i].c_str()) != nullptr);
            if (whitespace(pos)) {
                while (whitespace(pos))
                    ++pos;
                if (separator(pos))
                    ++pos;
            } else if (separator(pos))
                ++pos;
            while (whitespace(pos))
                ++pos;
        }
    }
    return newline ? 0 : 1;
}

// wait — block until every background job started with `&` has finished
int bi_wait(const std::vector<std::string> &a, ShellState &st) {
    reap_jobs(st);
    int result = 0;
    if (a.size() == 1) {
        for (auto &job : st.bg_jobs)
            result = job.reaped ? job.status : wait_for_job(job, st, false);
        return result;
    }
    for (size_t i = 1; i < a.size(); ++i) {
        std::string text = a[i];
        const bool index = text.starts_with('%');
        if (index)
            text.erase(0, 1);
        char *end = nullptr;
        errno = 0;
        long id = strtol(text.c_str(), &end, 10);
        if (text.empty() || end != text.c_str() + text.size() || errno || id <= 0) {
            std::cerr << "nash: wait: invalid job: " << a[i] << '\n';
            return 2;
        }
        ShellState::BgJob *selected = nullptr;
        for (size_t j = 0; j < st.bg_jobs.size(); ++j)
            if ((index && static_cast<long>(j + 1) == id) || (!index && st.bg_jobs[j].pid == id))
                selected = &st.bg_jobs[j];
        if (!selected) {
            std::cerr << "nash: wait: not a child job: " << a[i] << '\n';
            result = 127;
        } else
            result = selected->reaped ? selected->status : wait_for_job(*selected, st, false);
    }
    return result;
}

int bi_jobs(const std::vector<std::string> &, ShellState &st) {
    reap_jobs(st);
    for (size_t i = 0; i < st.bg_jobs.size(); ++i) {
        auto &job = st.bg_jobs[i];
        std::cout << '[' << i + 1 << "] "
                  << (job.reaped    ? "done"
                      : job.stopped ? "stopped"
                                    : "running")
                  << " " << job.pid << " " << job.cmd << '\n';
    }
    return 0;
}
int bi_job_control(const std::vector<std::string> &a, ShellState &st) {
    reap_jobs(st);
    ShellState::BgJob *job = nullptr;
    if (a.size() > 1) {
        std::string arg = a[1];
        size_t used = 0;
        int id;
        bool index = arg.starts_with('%');
        if (index)
            arg.erase(0, 1);
        try {
            id = std::stoi(arg, &used);
        } catch (...) {
            return 2;
        }
        if (used != arg.size() || id <= 0)
            return 2;
        for (size_t i = 0; i < st.bg_jobs.size(); ++i)
            if ((index && static_cast<int>(i + 1) == id) || (!index && st.bg_jobs[i].pid == id))
                job = &st.bg_jobs[i];
    } else
        for (auto &candidate : st.bg_jobs)
            if (!candidate.reaped)
                job = &candidate;
    if (!job || job->reaped) {
        std::cerr << "nash: no such active job\n";
        return 1;
    }
    if (a[0] == "fg" && st.interactive) {
        tcsetpgrp(0, job->pid);
        if (job->has_modes)
            tcsetattr(0, TCSADRAIN, &job->modes);
    }
    if (kill(-job->pid, SIGCONT) < 0)
        return 1;
    job->stopped = false;
    job->stopped_pids.clear();
    return a[0] == "bg" ? 0 : wait_for_job(*job, st, true);
}
} // namespace ang
