// config.h — loads ~/.config/nash/config.ang, theme.conf, plugins/*.ang.
// All config lines are plain nash commands, so ricing needs no new language.
#pragma once
#include "env.h"

namespace ang {

void apply_defaults(ShellState& st);
void load_config(ShellState& st);  // silent; never fatal

}  // namespace ang
