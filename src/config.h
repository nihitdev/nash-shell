// XDG Nash command configuration (with legacy TOML fallback), explicit startup commands and session
// history.
#pragma once
#include "env.h"
namespace ang {
void apply_defaults(ShellState &st);
void load_config(ShellState &st);
int bi_config(const std::vector<std::string> &args, ShellState &st);
} // namespace ang
