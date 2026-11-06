#include "completion.h"
#include "integrations.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <sstream>
#include <unistd.h>
extern char **environ;
namespace ang {
namespace {
struct Word {
    size_t begin = 0, end = 0;
    std::string text;
    bool quoted = false;
};
bool separator(char c) { return std::string(";|&\n").find(c) != std::string::npos; }
bool space(char c) { return c == ' ' || c == '\t'; }
bool safe_name(const std::string &s) {
    return std::none_of(s.begin(), s.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
std::string escape(const std::string &s, bool variable, bool tilde) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (std::string(" \t'\"\\$;&|(){}<>*?[]#`~").find(c) != std::string::npos &&
            !(variable && (c == '$' || c == '{' || c == '}')) && !(tilde && i == 0 && c == '~'))
            out += '\\';
        out += c;
    }
    return out;
}
int rank(const std::string &name, const std::string &prefix) {
    if (name.starts_with(prefix))
        return 0;
    auto lower = [](std::string s) {
        for (char &c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    auto n = lower(name), p = lower(prefix);
    if (n.starts_with(p))
        return 1;
    size_t pos = 0;
    for (char c : p) {
        pos = n.find(c, pos);
        if (pos == std::string::npos)
            return -1;
        ++pos;
    }
    return 2;
}
} // namespace
Completion complete_line(const std::string &line, size_t cursor, const ShellState &st,
                         bool query_git) {
    cursor = std::min(cursor, line.size());
    Completion result{cursor, cursor, {}};
    std::vector<Word> words;
    Word word;
    bool active = false, escaped = false, redirect = false;
    char quote = 0;
    // Decode only literal words. Completion must never run substitutions or aliases.
    for (size_t i = 0; i < cursor; ++i) {
        char c = line[i];
        if (!active) {
            if (space(c))
                continue;
            word = {i, i, {}, false};
            active = true;
        }
        if (escaped) {
            if (quote == '"' && std::string("$`\\\"\n").find(c) == std::string::npos)
                word.text += '\\';
            word.text += c;
            escaped = false;
        } else if (c == '\\' && quote != '\'') {
            escaped = true;
            if (word.text.empty())
                word.quoted = true;
        } else if (quote) {
            if (c == quote)
                quote = 0;
            else {
                if (quote != '\'' &&
                    (c == '`' || (c == '$' && i + 1 < cursor && line[i + 1] == '(')))
                    return result;
                word.text += c;
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
            word.quoted = true;
        } else if (c == '`' || (c == '$' && i + 1 < cursor && line[i + 1] == '(')) {
            return result;
        } else if (separator(c)) {
            words.clear();
            active = false;
            redirect = false;
        } else if (c == '<' || c == '>') {
            if (!word.text.empty())
                words.push_back(word);
            active = false;
            redirect = true;
        } else if (space(c)) {
            word.end = i;
            words.push_back(word);
            active = false;
        } else {
            word.text += c;
        }
    }
    if (!active)
        word = {cursor, cursor, {}, false};
    result.begin = word.begin;
    result.end = cursor;
    // Replace the whole current token when completing in its middle, preserving later words.
    char end_quote = quote;
    bool end_escape = escaped;
    while (result.end < line.size()) {
        char c = line[result.end];
        if (end_escape)
            end_escape = false;
        else if (c == '\\' && end_quote != '\'')
            end_escape = true;
        else if (end_quote) {
            if (c == end_quote)
                end_quote = 0;
        } else if (c == '\'' || c == '"')
            end_quote = c;
        else if (space(c) || separator(c) || c == '<' || c == '>')
            break;
        ++result.end;
    }
    auto prefix = word.text;
    bool variable =
        !word.quoted && prefix.starts_with('$') && prefix.find('/') == std::string::npos;
    bool tilde = !word.quoted && prefix.starts_with("~/");
    std::map<std::string, int> found;
    auto add = [&](const std::string &value, const std::string &match, const std::string &base) {
        if (!safe_name(value))
            return;
        int score = rank(match, base);
        if (score >= 0)
            found[value] = score;
    };
    if (variable) {
        auto base = prefix.substr(prefix.starts_with("${") ? 2 : 1);
        if (base.ends_with('}'))
            base.pop_back();
        auto add_var = [&](const std::string &name) {
            if (!name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char c) {
                    return std::isalnum(c) || c == '_';
                }))
                add(prefix.starts_with("${") ? "${" + name + "}" : "$" + name, name, base);
        };
        for (auto &[name, _] : st.vars)
            add_var(name);
        for (char **entry = environ; entry && *entry; ++entry)
            add_var(std::string(*entry).substr(0, std::string(*entry).find('=')));
    } else {
        size_t command_index = 0;
        while (command_index < words.size() &&
               words[command_index].text.find('=') != std::string::npos)
            ++command_index;
        bool first = command_index == words.size() && !redirect;
        std::string command = command_index < words.size() ? words[command_index].text : "";
        // Resolve simple command aliases for argument context, never execute their bodies.
        if (auto alias = st.aliases.find(command); alias != st.aliases.end()) {
            std::istringstream body(alias->second);
            body >> command;
            std::vector<Word> arguments;
            std::string argument;
            while (body >> argument)
                arguments.push_back({0, 0, argument, false});
            words.insert(words.begin() + static_cast<std::ptrdiff_t>(command_index + 1),
                         arguments.begin(), arguments.end());
        }
        auto slash = prefix.rfind('/');
        auto base = slash == std::string::npos ? prefix : prefix.substr(slash + 1);
        auto stem = slash == std::string::npos ? "" : prefix.substr(0, slash + 1);
        auto directory = stem.empty() ? "." : stem;
        if (tilde)
            directory = st.get("HOME") + directory.substr(1);
        bool directories = !redirect && command == "cd";
        size_t scanned = 0;
        auto scan = [&](const std::string &dir, bool executables) {
            std::error_code ec;
            for (std::filesystem::directory_iterator it(dir, ec), end;
                 it != end && !ec && scanned < 20000; it.increment(ec)) {
                ++scanned;
                const auto &entry = *it;
                auto name = entry.path().filename().string();
                if (!base.starts_with('.') && name.starts_with('.'))
                    continue;
                std::error_code entry_error;
                bool is_dir = entry.is_directory(entry_error);
                if ((first && slash == std::string::npos && !executables && !is_dir) ||
                    (directories && !is_dir) ||
                    (first && slash != std::string::npos && !executables && !is_dir &&
                     access(entry.path().c_str(), X_OK) != 0) ||
                    (executables && (is_dir || access(entry.path().c_str(), X_OK) != 0)))
                    continue;
                add(executables ? name : stem + name + (is_dir ? "/" : ""), name, base);
            }
        };
        // At command position only executables and navigable directories are useful.
        if (!first || slash != std::string::npos || st.autocd) {
            scan(directory, false);
        }
        if (first && slash == std::string::npos) {
            for (const auto *name :
                 {"cd",    "pwd",     "echo",    "printf", "export",       "unset",
                  "alias", "unalias", "source",  "exit",   "jobs",         "fg",
                  "bg",    "wait",    "history", "help",   "config",       "has",
                  "read",  "type",    "which",   "theme",  "integrations", "nash-prompt"})
                add(name, name, prefix);
            for (auto &[name, _] : st.aliases)
                add(name, name, prefix);
            for (auto &[name, _] : st.abbrs)
                add(name, name, prefix);
            std::istringstream paths(st.get("PATH"));
            std::string path;
            while (std::getline(paths, path, ':'))
                scan(path.empty() ? "." : path, true);
        } else if (!redirect && command == "git") {
            if (words.size() == command_index + 1 && slash == std::string::npos) {
                found.clear();
                for (const auto *name :
                     {"add",     "branch", "checkout", "cherry-pick", "clone",  "commit", "diff",
                      "fetch",   "init",   "log",      "merge",       "pull",   "push",   "rebase",
                      "restore", "show",   "stash",    "status",      "switch", "tag"})
                    add(name, name, prefix);
            } else if (words.size() > command_index + 1) {
                auto sub = words[command_index + 1].text;
                bool files_only =
                    std::any_of(words.begin() + static_cast<std::ptrdiff_t>(command_index + 2),
                                words.end(), [](const Word &w) { return w.text == "--"; });
                if (!files_only && (sub == "switch" || sub == "checkout" || sub == "merge" ||
                                    sub == "rebase" || sub == "show")) {
                    std::vector<std::string> args = {"git", "--no-optional-locks", "for-each-ref",
                                                     "--format=%(refname:short)", "refs/heads"};
                    if (sub != "switch") {
                        args.push_back("refs/remotes");
                        args.push_back("refs/tags");
                    }
                    if (sub == "switch" || sub == "merge" || sub == "rebase")
                        found.clear();
                    ToolResult refs;
                    if (query_git)
                        refs = capture_tool(args, 200);
                    if (refs.status == 0) {
                        if (sub == "switch" || sub == "merge" || sub == "rebase")
                            found.clear();
                        std::istringstream lines(refs.output);
                        std::string ref;
                        while (std::getline(lines, ref))
                            add(ref, ref, prefix);
                    }
                }
            }
        }
    }
    std::vector<std::pair<std::string, int>> sorted(found.begin(), found.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.second != b.second ? a.second < b.second : a.first < b.first;
    });
    for (auto &[value, score] : sorted) {
        // Exact prefixes win outright. Fuzzy fallbacks appear only if no prefix exists.
        if (!sorted.empty() && sorted.front().second < 2 && score != sorted.front().second)
            continue;
        result.candidates.push_back(escape(value, variable, tilde));
        if (result.candidates.size() == 256)
            break;
    }
    return result;
}
} // namespace ang
