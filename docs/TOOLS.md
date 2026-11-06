# Popular tools in Nash

This guide shows everyday commands and optional startup setup. Tools remain separate programs:
install the ones you want through their official instructions or your package manager. Nash
works without them. Run `integrations` to report availability, or `has TOOL` to test one silently.

Starship, Fastfetch, Eza and Bat have native support. Other tools use direct CLI commands or documented recipes. Aliases and lifecycle recipes below are explicit user
configuration, not a claim of complete Fish/Zsh-compatible integrations. Commands were checked
against installed CLI help and the linked upstream documentation; interactive workflows are
not all end-to-end tested. Tool versions can differ, so check `TOOL --help` when a flag changes.

## Fastfetch: system information when the terminal opens

Run it manually:

```sh
fastfetch
```

Add this to `~/.config/nash/config.nash` to display it once when starting each interactive Nash:

```sh
config set integrations.fastfetch.enabled true
```

The native startup adapter skips a missing executable and preserves command status. It runs
once during startup, not on every prompt or config reload. Use either this setting or the older
`has fastfetch && fastfetch` command recipe, not both.

For a manual shortcut instead:

```sh
has fastfetch && alias ff='fastfetch'
```

Use one startup invocation, not a `precmd` hook: precmd would run it before every prompt.
The native setting applies to the next interactive startup; manually invoking or sourcing a
Fastfetch command runs it immediately. To stop the banner, set `integrations.fastfetch.enabled false` and restart, or remove the
manual startup command if you used that recipe.

Fastfetch keeps its own JSONC file, normally `~/.config/fastfetch/config.jsonc`. Generate a config
with `fastfetch --gen-config` when needed; inspect any existing file first and follow the
generator's prompts. Test a separate config with `fastfetch --config /path/to/config.jsonc`.
Nash's `.nash` format does not replace Fastfetch's own configuration format. Find available
logo names with `fastfetch --list-logos`; inspect options with `fastfetch --help`.

Source: [Fastfetch configuration](https://github.com/fastfetch-cli/fastfetch/wiki/Configuration).

## Starship: your existing prompt configuration

Nash's own prompt is the default. Opt in later:

```sh
config set prompt.engine starship
```

Put that line in `config.nash` to keep it. Starship still reads `starship.toml` or the inherited
`STARSHIP_CONFIG`. Select a particular file with an export before choosing the engine:

```sh
export STARSHIP_CONFIG="$HOME/.config/starship/fish.toml"
config set prompt.engine starship
```

Nash directly supplies directory, command status, pipeline statuses, duration, width and jobs.
Do not add another shell's `starship init` command. Switch back with `config set prompt.engine native`.
Missing or failing Starship falls back to native. See [Nash configuration](CONFIGURATION.md)
for multiline, right-prompt and timeout behavior.

Sources: [Starship shell-agnostic operation](https://starship.rs/faq/),
[Starship configuration](https://starship.rs/config/).

## Eza: richer directory listings

```sh
eza -lah --icons=auto
eza --tree --level=2
eza -l --git
```

Enable managed shortcuts in `config.nash`:

```sh
config set integrations.eza.enabled true
```

Nash creates `ll`, `la`, and `lt` only when Eza is installed, preserves existing aliases, and
removes unchanged generated shortcuts when disabled. If you already define `ll`, remove or
rename that alias to let Nash manage it. Alternatively, use explicit aliases with a fallback:

```sh
alias ll='ls -lah'
has eza && alias ll='eza -lah --icons=auto'
```

`ll /tmp` appends the directory argument. Icons depend on font support. Keep plain `ls` available
while learning the different flags. `--git` adds Git status information to long listings.

Source: [Eza manual](https://github.com/eza-community/eza/blob/main/man/eza.1.md).

## Bat: highlighted file viewing

```sh
bat --paging=never README.md
bat --list-themes
```

Enable the managed viewer:

```sh
config set integrations.bat.enabled true
```

Nash sets `view` to Bat with paging disabled, falling back to the `batcat` executable. An existing
`view` alias is preserved. Missing Bat is skipped. Alternatively, define a dedicated viewer alias:

```sh
has bat && alias view='bat --paging=never'
has batcat && alias view='batcat --paging=never'
```

Then run `view README.md`. Some distributions use the executable name `batcat`. Disabling
paging makes short views convenient; omit `--paging=never` when you want Bat's normal pager.
A separate alias keeps ordinary `cat` behavior available for data-processing commands.

Source: [Bat README and configuration](https://github.com/sharkdp/bat).

## Zoxide: learn and find frequently visited directories

Register successful directory changes through Nash's explicit hook:

```sh
config unhook chpwd
has zoxide && config hook chpwd 'zoxide add "$PWD"'
```

`config unhook chpwd` clears all commands on that hook, so include other desired commands
afterward. It prevents duplicate registrations when reloading this recipe. The single quotes
preserve `$PWD` until the directory actually changes.

Run a few `cd` commands, then inspect matches:

```sh
zoxide query --list project
```

Jump to a known matching entry:

```sh
cd "$(zoxide query project)"
```

Choose keywords present in your database; a failed query cannot provide a destination.
Nash does not yet supply the full `z`/`zi` function wrappers. Do not paste `zoxide init fish`
or Bash initialization into Nash. This recipe records paths through a hook and uses Zoxide's
CLI directly.

Source: [Zoxide documentation](https://github.com/ajeetdsouza/zoxide).

## FZF: fuzzy selection

Run a selector:

```sh
fzf
find . -type f | fzf
```

Type to filter, use the arrow keys to move and Enter to select. Press Escape/Ctrl-C to cancel.
The selected value is printed to stdout. A noninteractive check is useful in scripts/tests:

```sh
printf 'alpha\nbeta\n' | fzf --filter beta
```

Nash's own Ctrl-R remains substring history search. FZF keybindings generated for Bash/Zsh/Fish
do not automatically become Nash bindings. A native selection-to-editor integration is future work;
running the external selector is supported as ordinary foreground command execution.

Source: [FZF usage and options](https://github.com/junegunn/fzf).

## Git: familiar commands and short aliases

```sh
git status --short
git diff
git log --oneline -10
```

Persistent aliases:

```sh
alias gs='git status --short'
alias gd='git diff'
alias gl='git log --oneline -10'
```

The native prompt shows a branch when its simple `.git/HEAD` lookup finds one; Starship's Git
modules handle richer prompt information. Git-aware Tab completion is not implemented yet.
These aliases do not create commits, push, or stage anything automatically.

Source: [Git reference](https://git-scm.com/docs).

## Yazi: terminal file manager

```sh
yazi
```

An optional launcher alias is straightforward:

```sh
has yazi && alias y='yazi'
```

This opens Yazi. It does not change Nash's working directory when Yazi exits. Upstream's
shell wrappers add that behavior, but Nash cannot run their Bash/Fish function syntax yet.
Do not advertise this plain alias as a full directory-changing adapter.

Source: [Yazi quick start and shell wrappers](https://yazi-rs.github.io/docs/quick-start/).

## Atuin: existing history search

```sh
atuin search
atuin --help
```

You can launch the external CLI, but Nash does not currently register its commands into Atuin
or connect Ctrl-R to Atuin. Nash uses its own history file and editor search. Atuin's Bash/Fish
initializers are not Nash scripts; do not source them here. Account/sync choices belong to Atuin,
not Nash, and are not required to use Nash.

Source: [Atuin documentation](https://docs.atuin.sh/).

## Direnv: inspect status; automatic import is not implemented

```sh
direnv status
```

Direnv needs a shell-specific hook to import/export environment changes. Nash has lifecycle
hooks but not a validated native Direnv environment adapter yet. Simply running `direnv`
does not change its parent shell's environment. Do not paste `direnv hook bash` or
`direnv hook fish` output into Nash, and do not assume `.envrc` activation is automatic.

Source: [Direnv hook setup](https://direnv.net/docs/hook.html).

## Prism and yoo: your developer tools

Both run as normal external commands. Inspect the installed versions before adding aliases:

```sh
prism --help
yoo --help
```

On the versions checked during this session, Prism exposes terminal theme commands, while
yoo exposes project/environment commands such as `yoo fetch`, `yoo project`, and `yoo doctor`.
A manual alias example is:

```sh
has yoo && alias yf='yoo fetch'
```

Nash does not yet synchronize its native prompt palette with Prism, import yoo-generated
completions, or register native adapters for either tool. Use their own CLI documentation/help
for options. No theme changes or account operations are performed automatically by Nash.

## A small combined config

```sh
config set prompt.engine native
alias ll='ls -lah'
alias gs='git status --short'
has eza && alias ll='eza -lah --icons=auto'
has bat && alias view='bat --paging=never'
has fastfetch && fastfetch
```

The full [example config](../examples/config.nash) leaves the Fastfetch banner commented out,
so copying it does not enable a startup banner without your choice. None of these tools is a
hard dependency of Nash. For more alias patterns, see [aliases](ALIASES.md).
