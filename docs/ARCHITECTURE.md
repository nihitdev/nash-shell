# Architecture

Nash retains ang.sh's small-module layout while moving runtime coordination out of `main`.

- `main`: CLI, interactive lifecycle, elapsed command timing, prompt and history coordination.
- `shell`: logical input, shared script/source evaluation, foreground terminal setup and job reaping.
- `lexer`: token boundaries, original byte ranges and shared bounded delimiter scanning; quote/substitution bodies survive parsing.
- `parser`: lists of and/or pipelines, commands, assignments and ordered redirections.
- `expand`: segment-aware quote removal, variables, substitution, field splitting and globbing.
- `executor`: prepares each command once, parent builtins, RAII descriptor/environment restoration,
  pipeline launch barrier, fork/exec, groups and wait statuses.
- `integrations`: direct argv capture with output cap and deadline; Starship protocol; lifecycle hooks.
- `config`: command-based config.nash startup, validated config builtin, legacy TOML fallback and history loading.
- `editor` / `highlight`: termios RAII, Unicode movement, display-cell positions, raw-input coloring,
  candidate-menu rendering, history suggestions and search.
- `completion`: literal-word context, safely escaped replacements, ranked prefix/fuzzy candidates,
  directory/executable filtering, variables, simple alias context and bounded local Git ref lookup.
- `suggestion`: recent-history-first append-only ghost text, with idle-debounced local completion fallback
  and external queries disabled; predictions are cached within the current edit.
- `prompt`: inherited template renderer, themes, ANSI-aware widths and native fallback.

No command is delegated to `/bin/sh` by Nash. External executables may of course invoke their
own interpreters. Parent state builtins run in-process for single foreground commands; piped
and background builtins run in children. Pipeline children block on a descriptor barrier until
the parent assigns process groups and gives foreground terminal ownership. Background pipelines
are tracked directly, without a supervisor process. Child signals reset before execution.

Prompts are captured before entering terminal editing. Starship is not called on keystrokes.
Editor redraws are buffered and use relative row positioning; they do not clear the full screen.
Right prompts appear only when they fit. Job status is polled/reaped at prompt boundaries and
while waiting for keyboard input; there is no background thread or busy loop.

Hooks preserve last command status and pipeline statuses. Recursion is suppressed. `chpwd`
fires only after a successful builtin cd, `preexec` before interactive evaluation and `precmd`
before prompt generation. Hooks run synchronously and can intentionally change shell state.

Full shell language support is the next architecture step: compound AST nodes must be added
before functions/control flow can be advertised. The current parser does not implement them.
