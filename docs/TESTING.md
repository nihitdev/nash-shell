# Testing

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DNASH_SANITIZE=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

Unit checks exercise lexer/parser structure, Unicode display widths, ANSI/OSC exclusion and
continuation detection. Execution tests cover commands, pipeline backpressure, redirections,
quote segments, assignments, substitution, statuses, background waits, Starship protocol,
fallback and deadline. Real Starship tests skip explicitly when it is absent.

PTY tests fork a controlling terminal and exercise real keyboard sequences, foreground reads,
interrupt, stop/resume, history, suggestions, completion and multiline input. They use isolated
XDG directories and never load the user's configuration. These checks are regression coverage,
not a POSIX conformance or terminal-emulator compatibility certification.

See the session report for actual runs. Do not infer a passing local run from the existence
of tests or a workflow file.

The documentation suite validates local handbook links and runs the shipped `config.nash`
plus Fastfetch/alias recipes against isolated executable fixtures. It checks that optional
startup output remains opt-in. This is recipe regression coverage, not a claim that every
third-party terminal program has a native adapter or has been interactively tested.

The reliability suite adds 86 Bash command/expansion comparisons and 36 read/IFS comparisons,
plus focused regression cases. It executes supported syntax in temporary directories and
avoids personal startup files. See [the audit](RELIABILITY.md) for failures, root causes and
fixes; passing these cases is not a claim of complete POSIX compliance.

The completion suite uses isolated filesystem fixtures to check ranking, quotes/escapes,
hidden files, directory filtering, variable names, tilde paths, command positions, replacement
ranges, Git subcommands/aliases, and the guarantee that substitutions are never executed.
It also checks theme newline decoding. PTY regressions cover actual Tab/Shift-Tab cycling,
quoted and fuzzy file completion, and switching to a completed Git branch under native and
Starship prompts.

Ghost-text regressions verify history priority, fresh filesystem/command/variable predictions,
append-only behavior, private leading-space suppression and disabled suggestions. PTY tests wait
for a newly discovered filename suffix without sending Tab, inspect its gray ANSI styling,
accept it with Right Arrow, and exercise Ctrl-C dismissal under native and Starship prompts.
