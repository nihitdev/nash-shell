// prompt.h — ricable prompt. Templates support placeholders:
//
//   {user} {host} {cwd} {base} {status} {char} {git} {mode}
//   {ok}      ✓ (green) or ✗ (red), reflecting the last exit status
//   {fail}    "" on success, red "✗ <status>" on failure (for right prompts)
//   {arrow}   ❯ colored green/red by the last exit status
//   {branch}  "" outside repos, " ⎇ <branch>" inside one
//   {time}    current time as HH:MM
//
// Colors: {black} {red} {green} {yellow} {blue} {magenta} {cyan} {white}
//   bright: {br_red} {br_green} {br_yellow} {br_blue} {br_magenta}
//           {br_cyan} {br_white}
//   256:    {orange} {purple} {pink} {gray} {teal} {lime}
//   backgrounds: {on_black} {on_red} {on_green} {on_yellow} {on_blue}
//           {on_magenta} {on_cyan} {on_white}
//   styles: {dim} {bold} {reset}
//
// Example: "{gray}{user}@{host}{reset} {bold}{blue}{cwd}{reset}{purple}{branch}{reset} {arrow} "
// Ricing = editing themes/<name>.conf or ~/.config/nash/theme.conf.
#pragma once
#include <string>
#include "env.h"

namespace ang {

// Expand a prompt template into ANSI-colored text.
std::string render_prompt(const std::string& tmpl, const ShellState& st);

// Load themes/<name>.conf or a direct path into st.prompt_left/right.
bool load_theme(const std::string& name_or_path, ShellState& st,
                std::string* err = nullptr);

// Visible length: ANSI escapes and UTF-8 continuation bytes don't count.
size_t visible_len(const std::string& s);

}  // namespace ang
