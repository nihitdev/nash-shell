// editor.h — minimal fish-style line editor (no external deps).
//
// Provides: history (Up/Down), Tab completion (builtins, PATH, files),
// ghost autosuggestion (Right / Ctrl-F to accept), inline highlighting,
// Emacs keys (Ctrl-A/E/U/K/W), and a right-aligned prompt.
//
// If stdin is not a TTY, read_line() falls back to std::getline.
#pragma once
#include "env.h"
#include <string>

namespace ang {

// Returns false on EOF (Ctrl-D on empty line).
bool read_line(ShellState &st, const std::string &left, const std::string &right, std::string *out);

void save_history(ShellState &st);

} // namespace ang
