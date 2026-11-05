# Troubleshooting

## `config: command not found`

You are running an older installed binary. Build the current source and reinstall:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
sudo cmake --install build-release --prefix /usr/local
```

Exit the old Nash process and run `nash` again. Existing processes do not replace themselves
when a new executable is installed. To test before installing, run `./build-release/nash`.

## Editing config.nash changes nothing

Check `echo "$XDG_CONFIG_HOME"`. If set, Nash reads that directory's `nash/config.nash`.
Otherwise it uses `~/.config/nash/config.nash`. Restart, or run `source` on the correct file.
The file is Nash code, not `[prompt]` TOML sections. Automatic loading happens in interactive
sessions, not ordinary `nash -c` or script runs. `--no-config` deliberately skips loading.

If `config.nash` exists, legacy `config.toml` is ignored. Keep the old file as a backup while
moving settings over. Run `config get prompt.engine` to see the active value. A later config
line can override an earlier value. Invalid setting values print an error.

## The prompt still resembles Fish or Zsh

Run `version` to confirm the active executable. Native Nash is the default. Select it with
`config set prompt.engine native`. Starship deliberately keeps your existing Starship prompt;
setting `prompt.engine starship` may therefore resemble another shell. `SHELL` often names
your account's login shell, not the currently running nested shell.

## Starship or a theme is missing

Run `has starship` followed by `echo $?`: zero means found. Run `integrations` for a report.
Nash falls back to native if Starship is missing/fails/times out. Starship reads its own config;
inspect `echo "$STARSHIP_CONFIG"` if it picks a different file than expected.

Named native themes need installed data files. Run from the checkout with an explicit theme
path, or install first. A custom `prompt.left` after the theme command overrides the theme.
Nerd Font symbols depend on your terminal font; true-color support depends on your terminal.

## An alias or startup tool fails

Define aliases on a previous line. Use `has TOOL && ...` for optional commands. Check the
actual binary name (`batcat` on some systems). Do not source `.bashrc`, `.zshrc`, `config.fish`,
`starship init fish`, or another shell's initialization output into Nash. Functions, loops,
`if`/`case`, Fish `$argv` and many POSIX constructs are currently unsupported.

## Fastfetch runs twice / startup is slow

Keep Fastfetch in the startup file, not a `precmd` hook. A precmd hook runs before every prompt.
Reloading `config.nash` reruns every startup command. Hooks append; use `config unhook POINT`
before registering a hook if you reload frequently. Starship's timeout bounds the prompt helper;
ordinary hooks and startup tools run synchronously and can take as long as their commands need.

## A terminal program stops or Ctrl-C behaves oddly

Use `jobs`, `fg`, `bg`, and `wait`. Nash has regression tests for terminal ownership and
stop/resume, but broader job-control stress testing remains future work. Reproduce without
personal config (`nash --no-config`) and include the command, Nash version, terminal and platform
in a bug report. Do not make Nash your login shell while it is early development software.
