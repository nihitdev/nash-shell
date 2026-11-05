// builtins.cpp
#include "builtins.h"
#include "executor.h"
#include "lexer.h"
#include "parser.h"
#include "prompt.h"
#include "util.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>

#ifndef NASH_VERSION
#define NASH_VERSION "0.1.3"
#endif

namespace ang {

static int need_args(const std::vector<std::string>& a, size_t n,
                     const std::string& use) {
    if (a.size() < n) {
        std::cerr << "usage: " << use << "\n";
        return 2;
    }
    return -1;
}

bool is_parent_builtin(const std::string& name) {
    return name == "cd" || name == "exit" || name == "logout" ||
           name == "export" || name == "unset" || name == "set" ||
           name == "alias" || name == "unalias" || name == "abbr" ||
           name == "posix" || name == "source" || name == "." ||
           name == "theme" || name == "autocd" || name == "history" ||
           name == "read" || name == "jobs" || name == "wait";
}

int run_builtin(const std::vector<std::string>& args, ShellState& st,
                bool* handled) {
    *handled = true;
    const std::string& c = args[0];
    if (c == "cd") return bi_cd(args, st);
    if (c == "exit" || c == "logout") return bi_exit(args, st);
    if (c == "echo") return bi_echo(args, st);
    if (c == "pwd") return bi_pwd(args, st);
    if (c == "export") return bi_export(args, st);
    if (c == "unset") return bi_unset(args, st);
    if (c == "set") return bi_set(args, st);
    if (c == "alias") return bi_alias(args, st);
    if (c == "unalias") return bi_unalias(args, st);
    if (c == "abbr") return bi_abbr(args, st);
    if (c == "posix") return bi_posix(args, st);
    if (c == "source" || c == ".") return bi_source(args, st);
    if (c == "history") return bi_history(args, st);
    if (c == "help") return bi_help(args, st);
    if (c == "version" || c == "--version") return bi_version(args, st);
    if (c == "true" || c == ":") return bi_true(args, st);
    if (c == "false") return bi_false(args, st);
    if (c == "type" || c == "which") return bi_type(args, st);
    if (c == "theme") return bi_theme(args, st);
    if (c == "env" || c == "printenv") return bi_env(args, st);
    if (c == "read") return bi_read(args, st);
    if (c == "jobs") return bi_jobs(args, st);
    if (c == "wait") return bi_wait(args, st);
    if (c == "autocd") {
        if (args.size() > 1) st.autocd = (args[1] == "on" || args[1] == "1");
        std::cout << "autocd " << (st.autocd ? "on" : "off") << "\n";
        return 0;
    }
    *handled = false;
    return 0;
}

int bi_cd(const std::vector<std::string>& a, ShellState& st) {
    (void)st;
    std::string dest = a.size() > 1 ? expand_tilde(a[1]) : home_dir();
    if (dest == "-") {
        const char* o = getenv("OLDPWD");
        dest = o ? o : home_dir();
        std::cout << dest << "\n";
    }
    char oldc[PATH_MAX];
    if (!getcwd(oldc, sizeof oldc)) oldc[0] = 0;
    if (chdir(dest.c_str()) != 0) {
        perror(("cd: " + dest).c_str());
        return 1;
    }
    setenv("OLDPWD", oldc, 1);
    char nc[PATH_MAX];
    if (getcwd(nc, sizeof nc)) setenv("PWD", nc, 1);
    return 0;
}

int bi_exit(const std::vector<std::string>& a, ShellState& st) {
    (void)st;
    g_should_exit = true;
    g_exit_code = a.size() > 1 ? atoi(a[1].c_str()) : st.last_status;
    return g_exit_code;
}

int bi_echo(const std::vector<std::string>& a, ShellState& st) {
    (void)st;
    bool nl = true, esc = false;
    size_t i = 1;
    // leading flags (-n, -e, -en, -ne) like bash
    for (; i < a.size(); ++i) {
        const std::string& f = a[i];
        if (f == "-n") nl = false;
        else if (f == "-e") esc = true;
        else if (f == "-en" || f == "-ne") { nl = false; esc = true; }
        else break;
    }
    bool first = true;
    for (; i < a.size(); ++i) {
        if (!first) std::cout << " ";
        first = false;
        if (!esc) { std::cout << a[i]; continue; }
        for (size_t j = 0; j < a[i].size(); ++j) {
            char c = a[i][j];
            if (c == '\\' && j + 1 < a[i].size()) {
                char d = a[i][j + 1];
                if (d == 'n') std::cout << '\n';
                else if (d == 't') std::cout << '\t';
                else if (d == '\\') std::cout << '\\';
                else std::cout << '\\' << d;
                ++j;
            } else {
                std::cout << c;
            }
        }
    }
    if (nl) std::cout << "\n";
    std::cout.flush();
    return 0;
}

int bi_pwd(const std::vector<std::string>&, ShellState&) {
    char b[PATH_MAX];
    if (getcwd(b, sizeof b)) { std::cout << b << "\n"; return 0; }
    perror("pwd");
    return 1;
}

// export NAME=value / export NAME  (POSIX style)
int bi_export(const std::vector<std::string>& a, ShellState& st) {
    if (a.size() == 1) {
        for (auto& kv : st.vars) std::cout << "export " << kv.first << "=\"" << kv.second << "\"\n";
        return 0;
    }
    for (size_t i = 1; i < a.size(); ++i) {
        std::string n, v;
        if (is_assign_word(a[i], &n, &v)) st.set_var(n, v, true);
        else if (is_valid_name(a[i])) {
            // export NAME: promote an environ-only variable too, keeping
            // its value instead of clobbering it to empty.
            std::string val;
            auto it = st.vars.find(a[i]);
            if (it != st.vars.end()) val = it->second;
            else if (const char* e = std::getenv(a[i].c_str())) val = e;
            st.set_var(a[i], val, true);
        } else {
            std::cerr << "export: invalid name: " << a[i] << "\n";
            return 1;
        }
    }
    return 0;
}

int bi_unset(const std::vector<std::string>& a, ShellState& st) {
    if (need_args(a, 2, "unset NAME...") == 2) return 2;
    for (size_t i = 1; i < a.size(); ++i) st.unset_var(a[i]);
    return 0;
}

// set NAME value...  (Ang style, fish-like). `set --posix on|off` toggles mode.
int bi_set(const std::vector<std::string>& a, ShellState& st) {
    if (a.size() >= 2 && a[1] == "--posix") {
        if (a.size() < 3) {
            std::cout << (st.posix ? "on" : "off") << "\n";
            return 0;
        }
        st.posix = (a[2] == "on" || a[2] == "1" || a[2] == "true");
        return 0;
    }
    if (need_args(a, 3, "set NAME value...  (ang syntax; POSIX: export NAME=value)") == 2) return 2;
    if (!is_valid_name(a[1])) { std::cerr << "set: invalid name: " << a[1] << "\n"; return 1; }
    // `set NAME` with no value errors; `set NAME ""` sets an empty value
    std::string v = join(std::vector<std::string>(a.begin() + 2, a.end()), " ");
    st.set_var(a[1], v, true);
    return 0;
}

int bi_alias(const std::vector<std::string>& a, ShellState& st) {
    if (a.size() == 1) {
        for (auto& kv : st.aliases) std::cout << "alias " << kv.first << "='" << kv.second << "'\n";
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
            if (it != st.aliases.end()) std::cout << "alias " << it->first << "='" << it->second << "'\n";
            else { std::cerr << "alias: not found: " << a[i] << "\n"; return 1; }
        } else {
            std::string n = a[i].substr(0, eq), v = a[i].substr(eq + 1);
            if (v.size() >= 2 && ((v.front() == '\'' && v.back() == '\'') || (v.front() == '"' && v.back() == '"')))
                v = v.substr(1, v.size() - 2);
            st.aliases[n] = v;
        }
    }
    return 0;
}

int bi_unalias(const std::vector<std::string>& a, ShellState& st) {
    if (need_args(a, 2, "unalias NAME...") == 2) return 2;
    for (size_t i = 1; i < a.size(); ++i) st.aliases.erase(a[i]);
    return 0;
}

// abbr add NAME EXPANSION | abbr list | abbr rm NAME  (fish-like)
int bi_abbr(const std::vector<std::string>& a, ShellState& st) {
    if (a.size() == 1 || a[1] == "list") {
        for (auto& kv : st.abbrs) std::cout << kv.first << " -> " << kv.second << "\n";
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

int bi_posix(const std::vector<std::string>& a, ShellState& st) {
    if (a.size() == 1) {
        std::cout << (st.posix ? "on" : "off") << "\n";
        return 0;
    }
    if (a[1] == "on" || a[1] == "off" || a[1] == "status") {
        if (a[1] != "status") st.posix = (a[1] == "on");
        std::cout << "posix " << (st.posix ? "on" : "off") << "\n";
        return 0;
    }
    std::cerr << "usage: posix [on|off|status]\n";
    return 2;
}

int bi_source(const std::vector<std::string>& a, ShellState& st) {
    if (need_args(a, 2, "source FILE") == 2) return 2;
    std::string f = expand_tilde(a[1]);
    std::ifstream in(f);
    if (!in) { std::cerr << "source: cannot open " << f << "\n"; return 1; }
    std::string line;
    while (std::getline(in, line)) {
        bool skip = false;
        std::string s = preprocess(line, st, &skip);
        if (skip || g_should_exit) continue;
        std::string lerr;
        auto toks = lex(s, &lerr);
        if (!lerr.empty()) { std::cerr << "nash: " << lerr << "\n"; continue; }
        JobList jobs;
        std::string perr;
        if (!parse(toks, st.posix, &jobs, &perr)) {
            std::cerr << "nash: " << perr << "\n";
            continue;
        }
        run_jobs(jobs, st);
        if (g_should_exit) break;
    }
    return st.last_status;
}

int bi_history(const std::vector<std::string>& a, ShellState& st) {
    if (a.size() > 1 && a[1] == "clear") { st.history.clear(); return 0; }
    for (size_t i = 0; i < st.history.size(); ++i)
        std::cout << i + 1 << "  " << st.history[i] << "\n";
    return 0;
}

int bi_help(const std::vector<std::string>&, ShellState&) {
    if (!use_color(STDOUT_FILENO)) {
        std::cout <<
            "nash " NASH_VERSION " — builtin help\n"
            "Modes:  Ang (default): `set FOO bar`   POSIX: `export FOO=bar`, `FOO=bar cmd`\n"
            "        Toggle: `posix on|off`, `set --posix on|off`, `nash --posix`\n"
            "Flow:   cmd | cmd, >, >>, <, 2>, 2>>, &>, 2>&1, &&, ||, ;, & (bg)\n"
            "Subst:  $(cmd) command substitution\n"
            "Input:  read [-p PROMPT] NAME...\n"
            "Vars:   $NAME ${NAME} $? $$ $!   Glob: * ? [...]\n"
            "Builtins: cd pwd exit echo export unset set alias unalias abbr posix\n"
            "          source history help version true false type theme env autocd\n"
            "          read jobs wait\n"
            "Ricing: ~/.config/nash/config.ang, themes/<name>.conf, plugins/*.ang\n"
            "Keys:   Tab complete, Right/Ctrl-F accept suggestion, Up/Down history,\n"
            "        Ctrl-A/E/U/K/W, !! / !$ history expansion\n";
        return 0;
    }
    const std::string B = "\033[1;36m";  // bright cyan: frame
    const std::string H = "\033[1;33m";  // bright yellow: section heads
    const std::string C = "\033[1;32m";  // bright green: syntax examples
    const std::string D = "\033[2m";     // dim: hints
    const std::string R = "\033[0m";
    const size_t W = 59;                 // content width between │ borders

    // Pad by *visible* width (ANSI codes and UTF-8 continuation bytes
    // are not columns), so the frame stays aligned in any terminal.
    auto dashes = [](size_t n) {
        std::string s;
        for (size_t i = 0; i < n; ++i) s += "\u2500";  // ─
        return s;
    };
    auto row = [&](const std::string& content) {
        size_t vis = visible_len(content);
        std::cout << "  " << B << "\u2502" << R << " " << content;
        if (vis < W) std::cout << std::string(W - vis, ' ');
        std::cout << " " << B << "\u2502" << R << "\n";
    };

    std::string hdr = "nash " NASH_VERSION;
    std::string top = "\u2500 " + hdr + " ";
    std::cout << "  " << B << "\u256d" << top
              << dashes(61 - visible_len(top)) << "\u256e" << R << "\n";

    row(H + "modes" + R);
    row("  Ang (default)  " + C + "set FOO bar" + R);
    row("  POSIX          " + C + "export FOO=bar" + R + "  " + D + "or" + R +
        "  " + C + "FOO=bar cmd" + R);
    row("  toggle: " + C + "posix on|off" + R + "  " + D + "or" + R + "  " +
        C + "nash --posix" + R);
    row(H + "flow" + R);
    row("  " + C + "cmd | cmd" + R + "   " + C + "> >> < 2> 2>&1 &>" + R +
        "   " + C + "&& || ; &" + R);
    row(H + "subst & input" + R);
    row("  " + C + "$(cmd)" + R + D + "  output becomes the word" + R);
    row("  " + C + "read [-p \"ask: \"] X Y" + R + D + "  line -> vars" + R);
    row(H + "variables" + R);
    row("  " + C + "$NAME ${NAME} $? $$ $!" + R + "   " + D + "glob:" + R +
        " " + C + "* ? [...]" + R);
    row(H + "builtins" + R);
    row("  cd pwd exit echo export unset set alias unalias");
    row("  abbr posix source history help version type theme");
    row("  env autocd true false read jobs wait");
    row(H + "ricing" + R);
    row("  " + D + "~/.config/nash/" + R + "config.ang theme.conf plugins/*.ang");
    row("  " + C + "theme rice" + R + "  " + D + "(default minimal rice power mono)" + R);
    row(H + "keys" + R);
    row("  " + C + "Tab" + R + " complete  " + C + "\u2192/Ctrl-F" + R +
        " accept  " + C + "\u2191\u2193" + R + " history");
    row("  " + C + "Ctrl-A/E/U/K/W" + R + " edit  " + C + "!! !$" + R +
        " last cmd/arg");

    std::cout << "  " << B << "\u2570" << dashes(61) << "\u256f" << R << "\n";
    return 0;
}

int bi_version(const std::vector<std::string>&, ShellState& st) {
    std::cout << st.shell_name << " " NASH_VERSION
              << (st.posix ? " (posix)" : " (ang)") << "\n";
    return 0;
}
int bi_true(const std::vector<std::string>&, ShellState&) { return 0; }
int bi_false(const std::vector<std::string>&, ShellState&) { return 1; }

int bi_type(const std::vector<std::string>& a, ShellState& st) {
    if (need_args(a, 2, "type NAME...") == 2) return 2;
    for (size_t i = 1; i < a.size(); ++i) {
        bool isb = is_parent_builtin(a[i]) || a[i]=="echo" || a[i]=="pwd" ||
                   a[i]=="true" || a[i]=="false" || a[i]=="type" || a[i]=="help" ||
                   a[i]=="version" || a[i]=="env" || a[i]=="which";        auto al = st.aliases.find(a[i]);
        auto ab = st.abbrs.find(a[i]);
        if (isb) std::cout << a[i] << " is a shell builtin\n";
        else if (al != st.aliases.end()) std::cout << a[i] << " is an alias: " << al->second << "\n";
        else if (ab != st.abbrs.end()) std::cout << a[i] << " is an abbreviation: " << ab->second << "\n";
        else std::cout << a[i] << " is an external command\n";
    }
    return 0;
}

int bi_env(const std::vector<std::string>&, ShellState& st) {
    for (auto& kv : st.vars) std::cout << kv.first << "=" << kv.second << "\n";
    return 0;
}

// read [-p PROMPT] NAME...  — read one line from stdin (fish/bash-like).
// One name: whole line. Several: split on whitespace, last gets the rest.
int bi_read(const std::vector<std::string>& a, ShellState& st) {
    std::string prompt;
    size_t i = 1;
    if (i < a.size() && a[i] == "-p" && i + 1 < a.size()) { prompt = a[++i]; ++i; }
    if (i >= a.size()) { std::cerr << "usage: read [-p PROMPT] NAME...\n"; return 2; }
    if (!prompt.empty()) { std::cerr << prompt; std::cerr.flush(); }  // stderr, like bash
    std::string line;
    if (!std::getline(std::cin, line)) {
        for (size_t k = i; k < a.size(); ++k) st.set_var(a[k], "", true);
        return 1;  // EOF
    }
    auto issp = [](char c) { return std::isspace((unsigned char)c) != 0; };
    size_t pos = 0;
    for (size_t k = i; k < a.size(); ++k) {
        std::string val;
        if (a.size() == i + 1) {
            val = line;  // single name: whole line, no splitting
        } else if (k == a.size() - 1) {
            while (pos < line.size() && issp(line[pos])) ++pos;
            val = line.substr(pos);  // last name gets the remainder
        } else {
            while (pos < line.size() && issp(line[pos])) ++pos;
            size_t s = pos;
            while (pos < line.size() && !issp(line[pos])) ++pos;
            val = line.substr(s, pos - s);
        }
        st.set_var(a[k], val, true);
    }
    st.last_status = 0;
    return 0;
}

// wait — block until every background job started with `&` has finished
int bi_wait(const std::vector<std::string>&, ShellState& st) {
    int rc = 0;
    for (auto& j : st.bg_jobs) {
        if (j.reaped) continue;
        int ws = 0;
        if (waitpid(j.pid, &ws, 0) < 0) continue;  // already gone somehow
        j.reaped = true;
        j.status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 128 + WTERMSIG(ws);
        rc = j.status;
    }
    return rc;
}

// jobs — list background jobs started with `&`
int bi_jobs(const std::vector<std::string>&, ShellState& st) {
    if (st.bg_jobs.empty()) { std::cout << "no background jobs\n"; return 0; }
    for (auto& j : st.bg_jobs) {
        if (!j.reaped) {
            int ws = 0;
            if (waitpid(j.pid, &ws, WNOHANG) != 0) {  // just finished
                j.reaped = true;
                j.status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 128 + WTERMSIG(ws);
            }
        }
        std::string state = j.reaped ? "done" : "running";
        std::printf("[%d] %-8s %s\n", (int)j.pid, state.c_str(), j.cmd.c_str());
    }
    return 0;
}

}  // namespace ang
