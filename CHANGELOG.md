# Changelog

## 0.2.0 — unreleased

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
