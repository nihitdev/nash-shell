#include "suggestion.h"
#include "completion.h"
#include <algorithm>
namespace ang {
std::string suggest_suffix(const std::string &line, const ShellState &st, bool fallback) {
    if (!st.suggestions_enabled || line.empty() || line.front() == ' ')
        return {};
    for (auto it = st.history.rbegin(); it != st.history.rend(); ++it) {
        if (it->size() > line.size() && it->starts_with(line) &&
            std::none_of(it->begin(), it->end(),
                         [](unsigned char c) { return (c < 32 && c != '\n') || c == 127; }))
            return it->substr(line.size());
    }
    if (!fallback)
        return {};
    auto completed = complete_line(line, line.size(), st, false);
    auto typed = line.substr(completed.begin);
    // Ghost text can only append: fuzzy rewrites belong to explicit Tab completion.
    // Do not invent an argument when the user has only typed a separating space.
    if (typed.empty() || (completed.begin == 0 && typed.size() < 2))
        return {};
    for (const auto &candidate : completed.candidates)
        if (candidate.size() > typed.size() && candidate.starts_with(typed))
            return candidate.substr(typed.size());
    return {};
}
} // namespace ang
