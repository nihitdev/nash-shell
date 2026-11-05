# Aliases and everyday shortcuts

Put aliases in `~/.config/nash/config.nash` so they load when Nash starts. Define an alias
on an earlier physical/logical line before using it: Nash preprocesses a logical command
before execution. `alias ll='ls -lah'; ll` on one line does not define-and-use it reliably.

## Useful starting aliases

```sh
alias ll='ls -lah'
alias la='ls -A'
alias gs='git status --short'
alias gd='git diff'
alias gl='git log --oneline -10'
alias ..='cd ..'
alias ...='cd ../..'
alias c='clear'
```

Then `ll /tmp` appends `/tmp` to the alias's command. `gs` runs Git status in the current
repository. `alias` lists aliases; `alias ll` inspects one; `unalias ll` removes one for the
current session. Edit the file too if you want a removal to persist after restarting.

Aliases do not implement functions, positional argument forwarding or conditionals. Avoid
self-referential aliases. For reusable multi-step work, write a `.nash` script using supported
commands rather than copying a Bash/Fish function.

## Use optional tools without breaking startup

`has NAME` succeeds when an external executable is available on PATH. It prints nothing.
`has NAME1 NAME2` succeeds only when every named tool exists. It checks executables, not aliases
or builtins. Combine it with `&&`:

```sh
alias ll='ls -lah'
has eza && alias ll='eza -lah --icons=auto'
has bat && alias view='bat --paging=never'
has batcat && alias view='batcat --paging=never'
has fastfetch && alias ff='fastfetch'
has yazi && alias y='yazi'
```

The base `ll` works even without Eza. `view README.md` uses Bat when present. Some distributions
call the executable `batcat`. If both exist, the later definition wins. Keeping `cat` unchanged
makes its behavior predictable in scripts; use a separate alias for rich file viewing.

## Environment variables and PATH

```sh
export EDITOR=nvim
export VISUAL=nvim
export PATH="$HOME/.local/bin:$PATH"
PROJECTS="$HOME/Projects"
```

Choose an editor actually installed on your system. `export` makes a variable available to
child processes. A plain assignment is shell-local unless the name was already exported.
Use `echo "$PROJECTS"` to inspect it and `unset PROJECTS` to remove it.

An alias expands when invoked, so deferred variables belong in single quotes:

```sh
alias projects='cd "$HOME/Projects"'
```

The directory must exist. Quoting preserves paths containing spaces. Single quotes keep
literal `$` text during definition; double quotes allow variable expansion.

## Fish-style abbreviations

The inherited `abbr` builtin can define command-word expansions:

```sh
abbr g='git'
abbr gst='git status --short'
```

These expand during command preprocessing; they are not Fish's complete abbreviation editor
or argument-position expansion system. Use aliases for straightforward persistent shortcuts.
