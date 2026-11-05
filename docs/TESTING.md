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
