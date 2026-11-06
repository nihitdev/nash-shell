<div align="center">

<h1>Nash Shell</h1>

<p><strong>A customizable Unix shell with a prompt of its own.</strong></p>
<p>Written in C++23. Configured with Nash commands. Built for Linux.</p>

<p>
  <a href="https://github.com/nihitdev/nash-shell/actions/workflows/ci.yml"><img src="https://github.com/nihitdev/nash-shell/actions/workflows/ci.yml/badge.svg" alt="Build and tests"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-7aa2f7?style=flat" alt="C++23">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-a6e3a1?style=flat" alt="MIT license"></a>
  <img src="https://img.shields.io/badge/status-early%20development-f9e2af?style=flat" alt="Early development">
</p>

<p>
  <a href="#quick-start"><strong>Quick start</strong></a> ·
  <a href="#configuration"><strong>Configuration</strong></a> ·
  <a href="docs/README.md"><strong>Handbook</strong></a> ·
  <a href="docs/TOOLS.md"><strong>Tools &amp; Fastfetch</strong></a> ·
  <a href="CONTRIBUTING.md"><strong>Contribute</strong></a>
</p>

<p><code>nash</code> · <code>~/.config/nash/config.nash</code><br>
No telemetry. No background daemon. No account required.</p>

</div>

---

```text
nash ~/Projects
❯ echo hello | tr a-z A-Z
HELLO
```

Nash starts with its own native prompt, editable history, suggestions, completion, and command
highlighting. Configure it with a `config.nash` startup file, add your favorite tools, or switch
to your existing Starship prompt when you want to.

> [!NOTE]
> Nash is in **early development**. Linux is tested; macOS and other POSIX systems are planned.
> It is not production-ready or fully POSIX-compatible. Try it inside your current terminal
> before considering it as a login shell. See [current limitations](#current-limitations).

## What you get

| | Available today |
|---|---|
| **A working shell** | External commands, pipelines, redirections, variables, quoting, globbing, and command substitution. |
| **Comfortable editing** | Gray history/command/path autosuggestions, syntax and command highlighting, ranked fuzzy/path/Git Tab completion with candidate cycling, reverse history search, and UTF-8 cursor movement. |
| **Your own configuration** | Nash commands for settings, aliases, environment variables, startup tools, hooks, and keybindings. |
| **Two prompt engines** | A native prompt by default; optional Starship with your existing configuration, status, duration, directory, and job context. |
| **Job control** | Background pipelines, foreground terminal handoff, Ctrl-C, Ctrl-Z, `jobs`, `fg`, `bg`, and `wait`. |
| **Native themes** | Minimal, Classic, Modern, Catppuccin, Rosé Pine, Tokyo Night, Gruvbox, Nord, Dracula, Monochrome, and the custom Nebula theme. |

## Quick start

You need a **C++23 compiler**, **CMake 3.20+**, **toml++ 3+**, and **Python 3** for tests.
External tools such as Starship and Fastfetch are optional. toml++ remains a dependency for
older Nash configuration files; new configurations use `config.nash`.

<details>
<summary><strong>Install build dependencies — Debian / Ubuntu</strong></summary>

```sh
sudo apt update
sudo apt install build-essential cmake libtomlplusplus-dev python3
```

</details>

<details>
<summary><strong>Install build dependencies — Arch Linux</strong></summary>

```sh
sudo pacman -S --needed base-devel cmake tomlplusplus python
```

</details>

Build and try Nash:

```sh
git clone https://github.com/nihitdev/nash-shell.git
cd nash-shell

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/nash
```

Type `exit` to return to your previous shell. To install globally:

```sh
sudo cmake --install build --prefix /usr/local
nash
```

Installation adds `nash` to your system; it does not change your login shell or replace a
running shell process. After an upgrade, exit the old Nash process and launch it again.

<details>
<summary><strong>Prefer a user-only installation?</strong></summary>

Configure the installation prefix before building so theme lookup uses the right location:

```sh
cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build-local --parallel
cmake --install build-local
```

Make sure `$HOME/.local/bin` is on your PATH. Launch with `~/.local/bin/nash` if needed.

</details>

## Configuration

Your main file is **`~/.config/nash/config.nash`**, or
`$XDG_CONFIG_HOME/nash/config.nash` when `XDG_CONFIG_HOME` is set.
It contains ordinary Nash commands:

```sh
# Keep Nash's own prompt.
config set prompt.engine native
config set autosuggestions.enabled true
config set highlighting.enabled true

# Everyday shortcuts.
alias ll='ls -lah'
alias gs='git status --short'

# Optional tools: activate them only when installed.
has eza && alias ll='eza -lah --icons=auto'
has bat && alias view='bat --paging=never'

# Uncomment for a system-information banner at startup.
# has fastfetch && fastfetch
```

Start with the commented [example config.nash](examples/config.nash), then explore the
[configuration reference](docs/CONFIGURATION.md) and [alias guide](docs/ALIASES.md).

Settings can also be changed in a running Nash:

```sh
config set completion.enabled true
config get prompt.engine
```

Put changes in the file to keep them across sessions. Reload it with
`source ~/.config/nash/config.nash`, or restart Nash. Reloading reruns startup commands;
clear a hook before registering it again if you reload frequently.

Personal config loads only for interactive sessions. `nash --no-config` skips it.
`config.nash` takes precedence over legacy `config.toml`; the old file is preserved as a
fallback when no `config.nash` exists. Bash, Zsh, and Fish startup files are not Nash scripts.

### Make the native prompt yours

After installing, select a theme:

```sh
config set prompt.theme catppuccin
```

Other names: `minimal`, `classic`, `modern`, `rose-pine`, `tokyo-night`, `gruvbox`, `nord`,
`dracula`, and `monochrome`. Templates support directory, branch, status, time, ANSI palette
colors, and true color. Native/editor colors respect `NO_COLOR` and `TERM=dumb`.
See [prompt customization](docs/CONFIGURATION.md#native-prompt-and-themes).

### Bring your Starship prompt

When you want Starship, add one line to `config.nash`:

```sh
config set prompt.engine starship
```

Nash finds Starship on PATH. Starship reads your existing `~/.config/starship.toml`, or the
path in `STARSHIP_CONFIG`. You can keep the prompt configuration you already use.

Nash supplies the last exit status, pipeline statuses, command duration, directory, terminal
width, and active job count. ANSI, Unicode, and multiline left prompts are preserved.
Single-line right prompts appear when space permits. Missing, failing, or timed-out Starship
falls back to the native prompt. Prompts are generated once per input, not on keystrokes.

No `starship init bash` or Fish initialization is needed. Shell-specific Starship modules and
custom commands may need an explicit supported interpreter in Starship's own configuration.
Read the [Starship setup guide](docs/TOOLS.md#starship-your-existing-prompt).

## Your favorite tools

Use the tools you already know, with explicit setup where needed:

| Tool | How it works in Nash |
|---|---|
| **Fastfetch** | Opt in with `config set integrations.fastfetch.enabled true`. |
| **Eza / Bat** | Opt-in managed shortcuts; existing aliases are preserved. |
| **Git** | Ordinary Git commands, convenient aliases, and prompt information. |
| **Zoxide** | Explicit directory-change hook and direct CLI queries. |
| **FZF / Yazi** | Run as foreground terminal programs. |
| **Atuin / Direnv** | External CLI commands; native history/environment adapters are pending. |
| **Prism / yoo** | Run your installed developer tools; native adapters are pending. |

**Starship, Fastfetch, Eza, and Bat have native support.** Running a tool or defining an alias does
not imply complete shell integration. `integrations` reports availability; `has TOOL` checks
for an external executable silently.

The [popular tools guide](docs/TOOLS.md) walks through setup, working examples, Fastfetch's own
configuration, and what still needs a native adapter.

## Learn the essentials

```sh
# Pipelines, variables, and exit status.
NAME=world
echo "hello $NAME" | tr a-z A-Z
false
echo $?

# Background work.
sleep 10 &
jobs
wait
```

Define aliases on an earlier line before using them. Leading-space commands stay out of
history, and concurrent sessions append history under a file lock. Reverse history search
is substring-based; the history limit bounds memory rather than disk growth.

| Key | Action |
|---|---|
| **Tab** | Complete commands and paths |
| **Right / Ctrl-F** | Accept the gray autosuggestion at the end of input |
| **Up / Down** | Browse history |
| **Ctrl-R** | Search older history |
| **Ctrl-A / Ctrl-E** | Move to the beginning / end |
| **Ctrl-C** | Cancel input or interrupt foreground work |
| **Ctrl-Z** | Stop foreground work; resume with `fg` or `bg` |
| **Ctrl-D** | Exit when input is empty |

See the [full key reference](docs/KEYBINDINGS.md) for editing, multiline input, paste handling,
and custom bindings.

## Website

The [documentation website source](website/README.md) lives in `website/`. It includes a
searchable handbook, tool support directory, and local config builder.

## Documentation

| Start here | Go further |
|---|---|
| [Getting started](docs/GETTING-STARTED.md) | [Architecture](docs/ARCHITECTURE.md) |
| [Configuration reference](docs/CONFIGURATION.md) | [Integration contracts](docs/INTEGRATIONS.md) |
| [Aliases and shortcuts](docs/ALIASES.md) | [Testing and sanitizers](docs/TESTING.md) |
| [Popular tools and Fastfetch](docs/TOOLS.md) | [Measured benchmarks](docs/BENCHMARKS.md) |
| [Editing and keybindings](docs/KEYBINDINGS.md) | [Changelog](CHANGELOG.md) |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | [Contributing](CONTRIBUTING.md) |

Read the [command reliability audit](docs/RELIABILITY.md) for reproduced failures, root causes,
fixes, and the Bash comparison suite.

The [handbook](docs/README.md) is also installed under `share/doc/nash/docs/` beneath your
installation prefix for offline reading.

## Current limitations

Nash is a working shell with an incomplete shell language. Functions, `if`/loops/`case`,
subshell syntax, heredocs, arithmetic/process substitution, arrays, traps, full parameter
operators, positional script arguments, asynchronous whole `&&`/`||` lists, and
`set -e` / pipefail are not implemented. Do not use it to run arbitrary Bash or POSIX scripts.

Editing moves by UTF-8 codepoint rather than grapheme cluster, and unusual emoji widths can
vary by terminal. Multiline history is session-only. Full tool-specific completion scripts, native Atuin
history, Direnv environment import, and a plugin ABI remain future work. Right prompts are
single-line. Job control needs broader stress testing before use as a login shell.

## Contributing

Bug reports, documentation improvements, and focused fixes are welcome. Read
[CONTRIBUTING.md](CONTRIBUTING.md), check the [architecture](docs/ARCHITECTURE.md), and run the
relevant tests before opening a pull request. Report sensitive issues through the process in
[SECURITY.md](SECURITY.md).

## Built on ang.sh

Nash is an attributed **fork of [ang.sh](https://github.com/0x251C08/ang.sh)** by **0x251C08**,
based on commit `fa22add8d9bdf7dd9fec9b29bbc4fb73a76764e7`, and maintained by **nihitdev**.
It retains upstream Git history, MIT copyright, and the internal `ang` namespace.
Lexer/parser interfaces, builtins, execution code, utilities, and native prompt templates
derive from that project, with substantial changes in Nash.
See [provenance and attribution](docs/ATTRIBUTION.md) for details.

<div align="center">

<p><strong>Nash Shell</strong> · Maintained by <a href="https://github.com/nihitdev">nihitdev</a></p>
<p>MIT licensed · Copyright 2026 0x251C08 and nihitdev · <a href="LICENSE">License</a></p>

</div>
