// util.h — tiny string/path helpers used across nash.
// Keep this dependency-free so new contributors only learn std::string.
#pragma once
#include <string>
#include <vector>

namespace ang {

std::string trim(const std::string& s);
std::vector<std::string> split_words(const std::string& s);
bool starts_with(const std::string& s, const std::string& prefix);
bool ends_with(const std::string& s, const std::string& suffix);

// NAME=value assignment test (POSIX style). Name: [A-Za-z_][A-Za-z0-9_]*
bool is_assign_word(const std::string& w, std::string* name = nullptr,
                    std::string* value = nullptr);
bool is_valid_name(const std::string& n);

std::string home_dir();
std::string config_dir();   // $XDG_CONFIG_HOME/nash or ~/.config/nash
std::string data_dir();     // $XDG_DATA_HOME/nash or ~/.local/share/nash
std::string expand_tilde(const std::string& p);

std::string join(const std::vector<std::string>& v, const std::string& sep);
std::string basename_of(const std::string& path);
std::string cwd_short();    // /home/u/Code -> ~/Code

// Quoting context of a word: None = unquoted, Single = '...', Double = "..." or $(...)
enum class QuoteKind { None, Single, Double };

// Quote context of one word: syntactic kind + whether literal quote chars
// appeared (lex strips them). Distinctes `""` (kept when empty) from
// `$(empty)` (dropped when empty, bash-like) — both are Double.
struct QuoteSpec {
    QuoteKind kind = QuoteKind::None;
    bool had_quotes = false;
};

// Color support: false when NO_COLOR is set, TERM=dumb, or fd is not a TTY.
bool use_color(int fd);
// "nash:" prefix for errors — red when stderr is a color terminal.
std::string err_prefix();

}  // namespace ang
