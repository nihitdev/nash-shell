// Quote-segment-aware variables, command substitution, field splitting and globbing.
// Raw token spelling survives parsing; captured commands have a nesting limit of eight.
#pragma once
#include "env.h"
#include "util.h"
#include <string>
#include <vector>

namespace ang {

std::vector<std::string> expand_word(const std::string &, ShellState &, bool split = true);
std::string expand_one(const std::string &word, ShellState &st, QuoteKind qk);
std::vector<std::string> expand_args(const std::vector<std::string> &words,
                                     const std::vector<QuoteSpec> &qspecs, ShellState &st);

} // namespace ang
