#pragma once
#include "env.h"
#include <string>
namespace ang {
// Append-only ghost text. History is immediate; filesystem fallback is requested after idle.
// Never forks, executes commands, or evaluates substitutions.
std::string suggest_suffix(const std::string &line, const ShellState &st, bool fallback = false);
} // namespace ang
