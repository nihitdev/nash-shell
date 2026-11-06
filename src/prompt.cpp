// prompt.cpp
#include "prompt.h"
#include "builtins.h"
#include "integrations.h"
#include "util.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cwchar>
#include <fstream>
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

namespace ang {

static std::string color(const std::string &n) {
    if (n == "black")
        return "\033[30m";
    if (n == "red")
        return "\033[31m";
    if (n == "green")
        return "\033[32m";
    if (n == "yellow")
        return "\033[33m";
    if (n == "blue")
        return "\033[34m";
    if (n == "magenta")
        return "\033[35m";
    if (n == "cyan")
        return "\033[36m";
    if (n == "white")
        return "\033[37m";
    if (n == "br_red")
        return "\033[91m";
    if (n == "br_green")
        return "\033[92m";
    if (n == "br_yellow")
        return "\033[93m";
    if (n == "br_blue")
        return "\033[94m";
    if (n == "br_magenta")
        return "\033[95m";
    if (n == "br_cyan")
        return "\033[96m";
    if (n == "br_white")
        return "\033[97m";
    if (n == "orange")
        return "\033[38;5;208m";
    if (n == "purple")
        return "\033[38;5;141m";
    if (n == "pink")
        return "\033[38;5;205m";
    if (n == "gray")
        return "\033[38;5;244m";
    if (n == "teal")
        return "\033[38;5;37m";
    if (n == "lime")
        return "\033[38;5;118m";
    if (n == "on_black")
        return "\033[40m";
    if (n == "on_red")
        return "\033[41m";
    if (n == "on_green")
        return "\033[42m";
    if (n == "on_yellow")
        return "\033[43m";
    if (n == "on_blue")
        return "\033[44m";
    if (n == "on_magenta")
        return "\033[45m";
    if (n == "on_cyan")
        return "\033[46m";
    if (n == "on_white")
        return "\033[47m";
    if (n == "dim")
        return "\033[2m";
    if (n == "bold")
        return "\033[1m";
    if (n == "reset")
        return "\033[0m";
    return "";
}

static std::string git_branch() {
    // Cheap: read .git/HEAD directly, no fork. Empty when not in a repo.
    char buf[4096];
    if (!getcwd(buf, sizeof buf))
        return "";
    std::string dir = buf;
    for (int up = 0; up < 6; ++up) {
        std::ifstream h(dir + "/.git/HEAD");
        if (h) {
            std::string line;
            std::getline(h, line);
            line = trim(line);
            if (starts_with(line, "ref: refs/heads/"))
                return line.substr(16);
            if (!line.empty())
                return line.substr(0, 8);
            return "";
        }
        size_t p = dir.find_last_of('/');
        if (p == std::string::npos || p == 0)
            break;
        dir = dir.substr(0, p);
    }
    return "";
}

static void replace_all(std::string &s, const std::string &from, const std::string &to) {
    size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos) {
        s.replace(p, from.size(), to);
        p += to.size();
    }
}

std::string render_prompt(const std::string &tmpl, const ShellState &st) {
    std::string s = tmpl;
    char host[256] = "?";
    gethostname(host, sizeof host);
    std::string hs = host;
    size_t dot = hs.find('.');
    if (dot != std::string::npos)
        hs = hs.substr(0, dot);
    const char *u = getenv("USER");
    std::string user = u ? u : "user";
    std::string cwd = cwd_short();
    std::string base = basename_of(cwd == "~" ? home_dir() : cwd);
    std::string gb = git_branch();
    bool ok = (st.last_status == 0);

    // current time as HH:MM
    char tbuf[6] = "--:--";
    {
        time_t now = time(nullptr);
        struct tm lt {};
        localtime_r(&now, &lt);
        strftime(tbuf, sizeof tbuf, "%H:%M", &lt);
    }

    auto clean = [](std::string value) {
        for (char &c : value)
            if (static_cast<unsigned char>(c) < 32 || c == 127 || c == '{' || c == '}')
                c = '?';
        return value;
    };
    user = clean(user);
    hs = clean(hs);
    cwd = clean(cwd);
    base = clean(base);
    gb = clean(gb);
    replace_all(s, "{user}", user);
    replace_all(s, "{host}", hs);
    replace_all(s, "{cwd}", cwd);
    replace_all(s, "{base}", base);
    replace_all(s, "{status}", std::to_string(st.last_status));
    replace_all(s, "{char}", std::string(ok ? "$" : "!"));
    replace_all(s, "{git}", gb.empty() ? "" : "(" + gb + ")");
    replace_all(s, "{branch}", gb.empty() ? "" : " ⎇ " + gb);
    replace_all(s, "{mode}", st.posix ? "◈posix " : "");
    {
        size_t njobs = 0;
        for (auto &j : st.bg_jobs)
            if (!j.reaped)
                ++njobs;
        replace_all(s, "{jobs}", std::to_string(njobs));
    }
    replace_all(s, "{time}", tbuf);
    replace_all(s, "{ok}", ok ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m");
    replace_all(s, "{fail}", ok ? "" : "\033[31m✗ " + std::to_string(st.last_status) + "\033[0m");
    replace_all(s, "{arrow}", std::string(ok ? "\033[32m" : "\033[31m") + "❯\033[0m");
    for (auto c :
         {"black",    "red",    "green",    "yellow",    "blue",    "magenta",    "cyan",
          "white",    "br_red", "br_green", "br_yellow", "br_blue", "br_magenta", "br_cyan",
          "br_white", "orange", "purple",   "pink",      "gray",    "teal",       "lime",
          "on_black", "on_red", "on_green", "on_yellow", "on_blue", "on_magenta", "on_cyan",
          "on_white", "dim",    "bold",     "reset"})
        replace_all(s, std::string("{") + c + "}", color(c));
    size_t offset = 0;
    while ((offset = s.find("{#", offset)) != std::string::npos) {
        if (offset + 8 < s.size() && s[offset + 8] == '}' &&
            s.substr(offset + 2, 6).find_first_not_of("0123456789abcdefABCDEF") ==
                std::string::npos) {
            auto rgb = s.substr(offset + 2, 6);
            auto ansi = "\033[38;2;" + std::to_string(std::stoi(rgb.substr(0, 2), nullptr, 16)) +
                        ";" + std::to_string(std::stoi(rgb.substr(2, 2), nullptr, 16)) + ";" +
                        std::to_string(std::stoi(rgb.substr(4, 2), nullptr, 16)) + "m";
            s.replace(offset, 9, ansi);
            offset += ansi.size();
        } else
            offset += 2;
    }
    return s;
}

size_t visible_len(const std::string &s) {
    size_t columns = 0;
    std::mbstate_t state{};
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '\033') {
            ++i;
            if (i < s.size() && s[i] == '[') {
                ++i;
                while (i < s.size() && !(s[i] >= '@' && s[i] <= '~'))
                    ++i;
                if (i < s.size())
                    ++i;
            } else if (i < s.size() && s[i] == ']') {
                ++i;
                while (i < s.size() && s[i] != '\a' &&
                       !(s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '\\'))
                    ++i;
                if (i < s.size())
                    i += s[i] == '\a' ? 1 : 2;
            } else if (i < s.size())
                ++i;
            continue;
        }
        wchar_t c;
        auto n = std::mbrtowc(&c, s.data() + i, s.size() - i, &state);
        if (n == 0 || n == static_cast<size_t>(-1) || n == static_cast<size_t>(-2)) {
            ++i;
            state = {};
            ++columns;
        } else {
            columns += static_cast<size_t>(std::max(0, ::wcwidth(c)));
            i += n;
        }
    }
    return columns;
}
static std::string strip_ansi(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
            i += 2;
            while (i < s.size() && !(s[i] >= '@' && s[i] <= '~'))
                ++i;
            if (i < s.size())
                ++i;
        } else
            out += s[i++];
    }
    return out;
}
std::string make_prompt(const ShellState &st, bool right) {
    if (st.prompt_engine == "starship") {
        winsize ws{};
        ioctl(1, TIOCGWINSZ, &ws);
        if (auto prompt = starship_prompt(st, right, ws.ws_col ? ws.ws_col : 80))
            return *prompt;
    }
    auto result = render_prompt(right ? st.prompt_right : st.prompt_left, st);
    return st.colors_enabled && use_color(1) ? result : strip_ansi(result);
}

static std::string theme_search_path(const std::string &name) {
    if (name.find('/') != std::string::npos)
        return name;
    // 1. user config, 2. ./themes (repo), 3. installed share
    std::vector<std::string> cands = {
        config_dir() + "/themes/" + name + ".conf",
        config_dir() + "/theme.conf",
        std::string(NASH_THEME_DIR) + "/" + name + ".conf",
        "/usr/local/share/nash/themes/" + name + ".conf",
        "/usr/share/nash/themes/" + name + ".conf",
    };
    for (auto &c : cands) {
        std::ifstream f(c);
        if (f)
            return c;
    }
    return "";
}

bool load_theme(const std::string &name_or_path, ShellState &st, std::string *err) {
    std::string path = name_or_path.find('/') != std::string::npos
                           ? name_or_path
                           : theme_search_path(name_or_path);
    if (path.empty()) {
        if (err)
            *err = "theme not found: " + name_or_path;
        return false;
    }
    std::ifstream in(path);
    if (!in) {
        if (err)
            *err = "cannot open theme: " + path;
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string k = trim(line.substr(0, eq));
        std::string v = trim(line.substr(eq + 1));
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
            v = v.substr(1, v.size() - 2);
        std::string decoded;
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == '\\' && i + 1 < v.size() && (v[i + 1] == 'n' || v[i + 1] == '\\')) {
                decoded += v[++i] == 'n' ? '\n' : '\\';
            } else
                decoded += v[i];
        }
        v = std::move(decoded);
        if (k == "prompt_left")
            st.prompt_left = v;
        else if (k == "prompt_right")
            st.prompt_right = v;
    }
    st.theme_name = name_or_path;
    return true;
}

// `theme` builtin lives here so prompt+theme stay in one module.
int bi_theme(const std::vector<std::string> &a, ShellState &st) {
    if (a.size() == 1) {
        // First line stays machine-readable (`theme | head -1`).
        std::cout << st.theme_name << "\n"
                  << "available: default minimal classic modern catppuccin rose-pine tokyo-night "
                     "gruvbox nord dracula monochrome nebula rice power mono\n";
        return 0;
    }
    std::string err;
    if (!load_theme(a[1], st, &err)) {
        std::cerr << "theme: " << err << "\n"
                  << "available: default minimal classic modern catppuccin rose-pine tokyo-night "
                     "gruvbox nord dracula monochrome nebula rice power mono\n";
        return 1;
    }
    return 0;
}

} // namespace ang
