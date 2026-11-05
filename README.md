# Nash Shell

A customizable C++23 Unix shell, maintained by **nihitdev**, with native Starship prompts.
Executable: `nash`. Linux is the initial tested platform. This is early development software,
not a production-ready or POSIX-certified shell.

Nash is a **fork of [ang.sh](https://github.com/0x251C08/ang.sh)** by 0x251C08,
based on commit `fa22add8d9bdf7dd9fec9b29bbc4fb73a76764e7`. Its lexer/parser interfaces,
builtin commands, execution model, native prompt templates and utilities derive from that project.
Nash retains upstream history and MIT copyright. The `ang` internal namespace is retained to make
this lineage clear. See [attribution](docs/ATTRIBUTION.md) for the changes made in this fork.

## Build and run

Requirements: a C++23 compiler, CMake >= 3.20, toml++ >= 3, and Python 3 for tests.
Starship and all other external tools are optional. No daemon, account, or telemetry.

```sh
# Debian / Ubuntu
sudo apt install build-essential cmake libtomlplusplus-dev python3
# Arch Linux
sudo pacman -S base-devel cmake tomlplusplus python

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/nash
./build/nash --no-config -c 'X=hello; echo "$X" | tr a-z A-Z'
```

Install without changing your login shell:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build --parallel
cmake --install build
```

## Starship: keep your existing prompt

Nash uses its own native prompt by default. To opt into Starship later, select
`engine = "starship"` in the configuration below. Nash then detects `starship` from PATH.
Starship itself reads your existing
`~/.config/starship.toml`, or `STARSHIP_CONFIG` when set. No conversion, duplicated prompt,
Bash init script, or hardcoded Nerd Font symbols are required.

Create `$XDG_CONFIG_HOME/nash/config.toml` (default `~/.config/nash/config.toml`):

```toml
[prompt]
engine = "starship"
fallback = "native"
timeout_ms = 1500
```

Nash passes last status, per-stage pipeline statuses, command duration in milliseconds,
physical and logical working directories, terminal width, and active job count to
`starship prompt`. It sets `STARSHIP_SHELL=nash` and a session key. Git modules run through
Starship itself. ANSI, Unicode, and multiline left prompts are preserved. `--right` output
is shown on the input row when it is a single line and fits without overlapping input.
Preview either engine with `nash-prompt --engine native` or `nash-prompt --engine starship`.
The prompt is generated once per input, not per keystroke. Missing, failing, or timed-out
Starship processes fall back to the native prompt; a missing right prompt simply disappears.

Starship does not recognize Nash as a named shell in every module. In particular, shell-specific
`[shell]` display settings and `[custom.*]` commands may need an explicit supported interpreter
in Starship's own configuration. Nash never executes Starship's output as shell code.
See [Starship's shell-agnostic integration guidance](https://starship.rs/faq/).

## Working features

- External commands, pipelines, `&&`, `||`, `;`, background jobs and exit statuses.
- Input/output/append/stderr redirection, `&>`, `2>&1`, `>&2`.
- Variables, command-local assignments, `$?`, `$$`, `$!`, quoting, escapes,
  `$(...)`, `${NAME:-fallback}`, tilde and filename expansion.
- `cd`, `pwd`, `export`, `unset`, `set`, aliases, abbreviations, `source`, `read`,
  history, `jobs`, `fg`, `bg`, and `wait`; run `help` for more.
- Foreground process groups, terminal handoff, Ctrl-C, Ctrl-Z, and resume.
- Syntax and command validation coloring, history ghost suggestions, path/executable Tab
  completion, Ctrl-R substring history search, Up/Down, Emacs editing keys, UTF-8 codepoint
  cursor movement, continuation input and bracketed paste.
- TOML settings, native theme templates, explicit `precmd`, `preexec`, and `chpwd` hooks.

POSIX-like assignment syntax is the default. `--ang` keeps the inherited `set NAME value`
mode; `--posix` selects assignments again. Configuration is loaded only in interactive mode;
`--no-config` disables it. Startup shell commands must be explicitly configured with
`[startup].file`; legacy `config.ang` and automatic plugin directory loading are not used.

Aliases are expanded before parsing a logical command, as in the inherited implementation:
define an alias on an earlier line before using it. Leading-space commands are omitted from
history. Concurrent history sessions append under a file lock. The history limit bounds
session memory; disk history is append-only and multiline records remain session-only.

## Customization and tools

See [examples/config.toml](examples/config.toml). The default engine is `native`; choose a
`theme` after installation. Shipped templates: minimal, classic, modern, catppuccin,
rose-pine, tokyo-night, gruvbox, nord, dracula, monochrome, plus inherited themes.
Native templates accept ANSI palette names and true-color `{#RRGGBB}` spans. `NO_COLOR` and `TERM=dumb` disable native and editor colors. Starship controls its own colors.

Keybindings map raw terminal sequences to `accept-suggestion`, `history-search`,
`beginning-of-line`, or `end-of-line`. Ctrl-R searches by substring; fuzzy search is planned.
Alt-Enter inserts a newline. Pasted newlines are buffered until Enter is pressed.

The `integrations` builtin reports availability of Starship, Zoxide, FZF, Atuin, Direnv,
Git, Eza, Bat, Yazi, Prism and yoo. **Only Starship has a native adapter.** The rest remain
ordinary optional external commands. Hooks accept Nash commands; do not source Bash/Fish
initialization scripts into Nash. See [integrations](docs/INTEGRATIONS.md).

## Known limits and next work

This is not a complete POSIX shell. Functions, `if`, loops, `case`, subshell syntax,
here-documents, arithmetic/process substitution, arrays, traps, full parameter operators,
positional script arguments, asynchronous whole `&&`/`||` lists, and `set -e` / pipefail are not implemented. Unsupported reserved
syntax is rejected. Do not use Nash to run arbitrary existing Bash/POSIX scripts.

Cursor movement is by codepoint, not Unicode grapheme cluster. Widths follow system
`wcwidth`; unusual emoji sequences and terminal-specific glyph widths can differ.
No native fuzzy/Git completion, Atuin history, Direnv export import, or plugin ABI yet.
Right prompts do not support multiple lines. Job control needs broader stress testing
before this should become a login shell. macOS and other POSIX systems remain planned.

See [architecture](docs/ARCHITECTURE.md), [testing](docs/TESTING.md),
[benchmarks](docs/BENCHMARKS.md), [contributing](CONTRIBUTING.md), and [changelog](CHANGELOG.md).

## License

MIT. Copyright 2026 0x251C08 and nihitdev. See [LICENSE](LICENSE).
