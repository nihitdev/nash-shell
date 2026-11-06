# Changelog

## 0.2.0 — unreleased

- Add Nebula, a custom multiline native theme; decode newline and backslash escapes in theme files.
- Extract context-aware completion with prefix/case-insensitive/fuzzy ranking, safe quoted filenames,
  directory-only cd, variables, command positions, simple aliases and local Git refs.
- Add a compact candidate menu with repeated Tab/Shift-Tab cycling and completion regression tests.

- Audit supported commands against Bash and add a dedicated reliability suite.
- Fix whole-token aliases, assignment previews, substitution statuses, recursive defaults and IFS splitting.
- Share bounded delimiter scanners; preserve significant escaped whitespace and substitution comments.
- Fix standard descriptor prefixes, read EOF/buffering/stdin ownership and Ctrl-C recovery.
- Validate exit/cd/type/wait behavior, forward env arguments, and fix completion at the input start.

- Add opt-in native Fastfetch startup and managed Eza/Bat shortcuts; preserve user aliases and skip absent tools.
- Add an Arch-inspired documentation website with searchable handbook, tool directory, themes and local config builder.

- Load config.nash as the primary shell startup file, with validated config and has builtins.
- Add a user handbook covering configuration, aliases, Fastfetch/popular tools, keys and troubleshooting.
- Keep legacy TOML as a fallback only when config.nash is absent.

- Use Nash's own native prompt by default; enable Starship explicitly in TOML.

- Establish Nash as an attributed fork of ang.sh; retain upstream history and MIT copyright.
- Move build to C++23; remove bundled package builds and duplicated source trees.
- Add native Starship argv integration, real prompt context, timeout and native fallback.
- Add XDG TOML configuration and explicit startup / preexec / precmd / chpwd hooks.
- Preserve quote segments through expansion; split unquoted results and retain quoted empties.
- Prepare command substitutions once per command rather than running them twice.
- Restore command-local builtin environments and descriptors on redirection failure.
- Add foreground process groups, launch barrier, stopped jobs, fg/bg and pipeline statuses.
- Track stopped pipeline stages individually to prevent stale stop events on resume.
- Exclude terminal-control filenames from completion candidates.
- Replace byte-based terminal editing with UTF-8 movement and cell-aware prompt placement.
- Add reverse history search, bracketed paste, continuation input and locked history append.
- Add unit, execution and PTY tests, sanitizers, formatting and static analysis configuration.

Functions and compound shell control flow remain future work. No production-readiness claim.
