#pragma once
#include "env.h"
#include <string>
#include <vector>
namespace ang {
struct Completion {
    size_t begin = 0, end = 0;
    std::vector<std::string> candidates; // Safely escaped replacement words, ranked best first.
};
// Never evaluates shell source. Cursor and replacement ranges are byte offsets.
Completion complete_line(const std::string &line, size_t cursor, const ShellState &st,
                         bool query_git = true);
} // namespace ang
