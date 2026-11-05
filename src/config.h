// XDG TOML configuration, explicit startup commands and session history.
#pragma once
#include "env.h"
namespace ang {
void apply_defaults(ShellState &st);
void load_config(ShellState &st);
} // namespace ang
