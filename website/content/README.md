# Nash user handbook

New to Nash? Read these in order:

1. [Getting started](GETTING-STARTED.md): install, launch, run commands and return to your old shell.
2. [Configure Nash](CONFIGURATION.md): `config.nash`, every supported setting, prompt themes and hooks.
3. [Aliases and everyday commands](ALIASES.md): shortcuts, environment variables and safe optional-tool checks.
4. [Popular tools](TOOLS.md): Fastfetch, Starship, Eza, Bat, Zoxide, FZF, Git, Yazi and more.
5. [Editing and keys](KEYBINDINGS.md): completion, suggestions, history, multiline input and custom keys.
6. [Troubleshooting](TROUBLESHOOTING.md): old installed binaries, prompts, config errors and missing tools.

Start with the commented [example config.nash](../examples/config.nash). It uses the same Nash
commands you can run interactively. No TOML is required for new Nash configurations.

For contributors: [architecture](ARCHITECTURE.md), [tests](TESTING.md),
[integration contracts](INTEGRATIONS.md), [benchmarks](BENCHMARKS.md),
[provenance](ATTRIBUTION.md) and [contributing](../CONTRIBUTING.md).

Nash is early development software. Functions, loops, if/case and many POSIX/Bash constructs
are not implemented yet. Examples here avoid those constructs. Never paste another shell's
initialization script into Nash without checking its syntax.
