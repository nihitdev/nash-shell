// Nash renderer: preserve every input byte while adding color spans.
#include "highlight.h"
#include "integrations.h"
#include "util.h"
#include <set>
namespace ang {
std::string highlight_line(const std::string &line, const ShellState &st) {
    static const std::set<std::string> builtins = {
        "cd",    "pwd",          "echo",        "exit",   "export", "unset", "set",
        "alias", "unalias",      "abbr",        "posix",  "source", ".",     "history",
        "help",  "version",      "true",        "false",  "type",   "which", "theme",
        "env",   "printenv",     "autocd",      "read",   "jobs",   "fg",    "bg",
        "wait",  "integrations", "nash-prompt", "config", "has"};
    std::string out;
    bool first = true;
    for (size_t i = 0; i < line.size();) {
        char c = line[i];
        if (c == ' ' || c == '\t' || c == '\n') {
            out += c;
            ++i;
            continue;
        }
        if (c == '#') {
            out += "\033[2m" + line.substr(i) + "\033[0m";
            break;
        }
        if (c == '\'' || c == '"') {
            char q = c;
            size_t begin = i++;
            while (i < line.size()) {
                if (line[i] == '\\' && q == '"' && i + 1 < line.size())
                    i += 2;
                else if (line[i++] == q)
                    break;
            }
            out += "\033[33m" + line.substr(begin, i - begin) + "\033[0m";
            first = false;
            continue;
        }
        if (std::string("|&;<>").find(c) != std::string::npos) {
            out += "\033[36m" + std::string(1, c) + "\033[0m";
            ++i;
            if (c == '|' || c == '&' || c == ';')
                first = true;
            continue;
        }
        size_t begin = i;
        while (i < line.size() && std::string(" \t\n'\"|&;<>").find(line[i]) == std::string::npos) {
            if (line[i] == '\\' && i + 1 < line.size())
                i += 2;
            else
                ++i;
        }
        auto word = line.substr(begin, i - begin);
        std::string color;
        if (first && !is_assign_word(word)) {
            bool valid = builtins.contains(word) || st.aliases.contains(word) ||
                         st.abbrs.contains(word) || find_executable(word).has_value();
            color = valid ? "\033[32m" : "\033[31m";
            first = false;
        } else if (word.starts_with('$'))
            color = "\033[35m";
        else if (word.starts_with('-'))
            color = "\033[96m";
        out += color + word + (color.empty() ? "" : "\033[0m");
    }
    return out;
}
std::string autosuggest(const std::string &line, const ShellState &st) {
    if (!st.suggestions_enabled || line.empty())
        return {};
    for (auto i = st.history.rbegin(); i != st.history.rend(); ++i)
        if (i->size() > line.size() && i->starts_with(line))
            return i->substr(line.size());
    return {};
}
} // namespace ang
