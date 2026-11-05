// util.cpp
#include "util.h"
#include <cstdlib>
#include <cctype>
#include <unistd.h>

namespace ang {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> split_words(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (std::isspace((unsigned char)c)) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool starts_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}
bool ends_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() &&
           s.compare(s.size() - p.size(), p.size(), p) == 0;
}

bool is_valid_name(const std::string& n) {
    if (n.empty() || !(std::isalpha((unsigned char)n[0]) || n[0] == '_'))
        return false;
    for (char c : n)
        if (!(std::isalnum((unsigned char)c) || c == '_')) return false;
    return true;
}

bool is_assign_word(const std::string& w, std::string* name,
                    std::string* value) {
    size_t eq = w.find('=');
    if (eq == std::string::npos || eq == 0) return false;
    std::string n = w.substr(0, eq);
    if (!is_valid_name(n)) return false;
    if (name) *name = n;
    if (value) *value = w.substr(eq + 1);
    return true;
}

std::string home_dir() {
    const char* h = std::getenv("HOME");
    return h ? h : "/";
}

std::string expand_tilde(const std::string& p) {
    if (p == "~") return home_dir();
    if (starts_with(p, "~/")) return home_dir() + p.substr(1);
    return p;
}

std::string config_dir() {
    const char* x = std::getenv("XDG_CONFIG_HOME");
    if (x && *x) return std::string(x) + "/nash";
    return home_dir() + "/.config/nash";
}

std::string data_dir() {
    const char* x = std::getenv("XDG_DATA_HOME");
    if (x && *x) return std::string(x) + "/nash";
    return home_dir() + "/.local/share/nash";
}

std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string o;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) o += sep;
        o += v[i];
    }
    return o;
}

std::string basename_of(const std::string& path) {
    size_t p = path.find_last_of('/');
    return p == std::string::npos ? path : path.substr(p + 1);
}

std::string cwd_short() {
    char buf[4096];
    std::string cwd = getcwd(buf, sizeof buf) ? buf : "?";
    std::string h = home_dir();
    if (cwd == h) return "~";
    if (starts_with(cwd, h + "/")) return "~" + cwd.substr(h.size());
    return cwd;
}

bool use_color(int fd) {
    if (std::getenv("NO_COLOR")) return false;
    const char* t = std::getenv("TERM");
    if (t && std::string(t) == "dumb") return false;
    return isatty(fd) != 0;
}

std::string err_prefix() {
    return use_color(STDERR_FILENO) ? "\033[31mnash\033[0m: " : "nash: ";
}

}  // namespace ang
