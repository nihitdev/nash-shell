# Configuring Nash with config.nash

## File and loading order

The main file is **`~/.config/nash/config.nash`**. When `XDG_CONFIG_HOME` is set, it is
`$XDG_CONFIG_HOME/nash/config.nash`. It contains ordinary Nash commands and comments beginning
with `#`, like a shell startup file. Nash loads it for interactive sessions only.

Startup applies built-in defaults, executes `config.nash`, sources an additional
`startup.file` if explicitly configured, then loads command history. If `config.nash` exists,
legacy `config.toml` is ignored completely. If it is absent, older TOML configs still work.
Nash does not rename, overwrite or automatically convert your old config. Starship's own TOML
configuration remains Starship's format.

`nash --no-config` skips personal config and startup files. Running `nash -c '...'` or a script
also skips automatic personal configuration. You can still explicitly `source` a file.

## Change and inspect settings

```sh
config set prompt.engine native
config get prompt.engine
config set autosuggestions.enabled false
config get autosuggestions.enabled
```

Changes apply to the running shell. To retain them for new terminals, put the commands in
`config.nash`. Nash does not automatically rewrite your config after interactive changes.
Run `config --help` for the command interface. Unknown settings, invalid booleans, invalid
ranges and unsupported actions report an error and leave the previous setting intact.

| Key | Accepted values | Default / effect |
|---|---|---|
| `prompt.engine` | `native`, `starship` | `native` |
| `prompt.theme` | installed theme name or theme file path | built-in Nash prompt initially |
| `prompt.left` | quoted template string | Nash label, directory, branch and status-aware arrow |
| `prompt.right` | quoted template string | empty |
| `prompt.timeout_ms` | integer 50–10000 | 1500; Starship helper deadline |
| `colors.enabled` | `true`/`false`, `on`/`off` | true; native prompt/editor colors |
| `autosuggestions.enabled` | boolean | true |
| `highlighting.enabled` | boolean | true |
| `completion.enabled` | boolean | true |
| `history.enabled` | boolean | true |
| `history.path` | quoted path; `~` supported | `$XDG_DATA_HOME/nash/history` or `~/.local/share/nash/history` |
| `history.limit` | integer 1–1000000 | 5000; in-memory limit, not disk rotation |
| `editor.autocd` | boolean | false; enter a directory by typing its name |
| `startup.file` | quoted path | empty; optional additional Nash script |

## Native prompt and themes

```sh
config set prompt.engine native
config set prompt.theme catppuccin
```

Installed themes: minimal, classic, modern, catppuccin, rose-pine, tokyo-night, gruvbox,
nord, dracula and monochrome, plus inherited default/rice/power/mono templates. Themes need
the installed data files; when running only from a checkout, pass an explicit path such as
`config set prompt.theme ./themes/catppuccin.conf` from the repository root.

Prompt templates accept `{cwd}`, `{base}`, `{user}`, `{host}`, `{branch}`, `{status}`,
`{jobs}`, `{time}`, `{arrow}` and `{fail}`. Colors include `{blue}`, `{cyan}`, `{gray}`,
`{bold}`, `{reset}` and true color such as `{#cba6f7}`. A literal multiline template works:

```sh
config set prompt.left '{cyan}nash{reset} {cwd}
{fail}{arrow} '
config set prompt.right '{dim}{time}{reset}'
```

Set the theme first, then custom left/right templates; later commands override earlier ones.
Right prompts display only when they fit on one input row. `NO_COLOR` or `TERM=dumb` disables
native/editor colors. Unicode glyph appearance depends on your terminal font and `wcwidth`.

## Switch to Starship later

```sh
config set prompt.engine starship
```

Starship reads its own config: normally `~/.config/starship.toml`, or the path in
`STARSHIP_CONFIG`. For example:

```sh
export STARSHIP_CONFIG="$HOME/.config/starship/fish.toml"
config set prompt.engine starship
```

You do not need to run `starship init bash` or source Fish initialization code. Nash passes
status, directory, duration, width, pipeline statuses and jobs directly. To switch back,
set `prompt.engine native`. Use `nash-prompt --engine native` or `--engine starship` to preview
without changing the active engine. The preview builtin prints rendered prompt text.

## Hooks, keys and extra files

```sh
# Clear first so reloading does not register the same hook twice.
config unhook chpwd
has zoxide && config hook chpwd 'zoxide add "$PWD"'
config bind Ctrl-F accept-suggestion
config bind Ctrl-R history-search
source ~/.config/nash/aliases.nash
```

The last line requires that file to exist; omit it until you create it. `config hook POINT
'COMMAND'` appends a Nash command. Points are `preexec` (before interactive commands),
`precmd` (before prompts) and `chpwd` (after successful cd). Hooks run synchronously, preserve
the preceding command's status and suppress hook recursion. Use single quotes around deferred
variables so they expand when the hook runs. `config unhook POINT` clears a point.

Bindings support `Ctrl-A`, `Ctrl-E`, `Ctrl-F`, `Ctrl-R`, `Right`, or a literal terminal sequence.
Actions: `accept-suggestion`, `history-search`, `beginning-of-line`, `end-of-line`.
`config unbind KEY` removes a custom override and restores built-in handling for that key.
See [keybindings](KEYBINDINGS.md).

## Compatibility and troubleshooting

A `config.nash` file can use supported assignments, aliases, commands, pipes, source and
`&&`/`||`. It cannot use Fish `set -g`, `$argv`, `end`, Bash functions or `if`/`for` yet.
See [troubleshooting](TROUBLESHOOTING.md) if `config` is reported as an unknown command: your
installed executable may predate this feature and needs rebuilding/reinstallation.
