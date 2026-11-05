# Nash development session — 2026-11-05

The project direction was changed at the user's request: improve the cloned ang.sh codebase
as **Nash Shell**, rather than continue an independent Astra implementation.

## Repository and provenance

- Working project: `/mnt/storage/Projects/nash-shell`, executable `nash`.
- Discarded session-created `/mnt/storage/Projects/astra-shell` directory removed as requested.
- Original `/mnt/storage/Projects/ang.sh` remains unchanged (clean Git status verified).
- Upstream history and MIT copyright preserved; attribution is explicit in README and LICENSE.
- Public repository created: https://github.com/nihitdev/nash-shell; `main` pushed successfully.
- No release tags or binaries published. Existing upstream history is not a Nash release.

## Files created and modified

- Build/tooling: CMakeLists.txt, .gitignore, .clang-format, .clang-tidy, CI workflow.
- Added runtime modules: src/integrations.{h,cpp}, src/shell.{h,cpp}.
- Updated inherited source: main, env, lexer, parser, executor, expand, builtins, config,
  editor, highlight, prompt and util, with source namespace retained as `ang`.
- Added tests: unit.cpp, execution.py, interactive.py, benchmark.py.
- Added configuration and themes: examples/config.toml and ten native theme templates;
  restored inherited templates from upstream's packaged copies.
- Added/replaced project documentation: README, CONTRIBUTING, SECURITY, CHANGELOG, architecture,
  attribution, integrations, testing, benchmark results and packaging instructions.
- Removed upstream bundled binaries, nested package build trees, upstream release workflow
  and release-download installer from the Nash working tree. Original repository is intact.

## Functional changes

Direct Starship prompt integration passes status, pipeline statuses, milliseconds elapsed,
directory context, width and active jobs. Existing Starship configuration stays owned by
Starship. Supports Unicode/ANSI/multiline output, right prompts when they fit, and bounded
helper execution with native fallback. No Starship rendering occurs on editor keystrokes.

TOML config follows XDG conventions. Explicit lifecycle hooks preserve previous command status.
Runtime improvements include quote-segment expansion, field splitting, single-evaluation
command substitution, environment/descriptor restoration, expanded redirect targets,
process groups, foreground terminal handoff and stopped-job fg/bg support. Editor improvements
include codepoint cursor movement, display-cell width, completion, syntax coloring,
history suggestions/search, bracketed paste and continuation input. History appends under a lock.

## Actual verification

- GCC 16.2.1, CMake, C++23: Debug, Release and ASan/UBSan builds succeeded.
- Debug CTest: all 3 suites passed in 15.04 s (unit executable, 25 execution tests, 19 PTY tests).
- ASan/UBSan CTest: all 3 suites passed in 17.41 s; no sanitizer diagnostics reported.
- Release: 25 execution tests passed in 2.154 s; 19 PTY tests passed in 6.746 s.
- Installed with DESTDIR staging; installed `nash --version` and an execution pipeline succeeded.
- clang-format 18.1.8 dry-run check passed, matching CI's formatter version.
- clang-tidy 23 completed for integrations.cpp, executor.cpp and editor.cpp with exit 0.
  It reported nonfatal portability/style, signed-bitwise, optional-access and performance
  warnings; it is not warning-clean. A follow-up run also covered shell.cpp.
- Real installed Starship tested with temporary configs: status, Git modules, Unicode,
  multiline rendering. Fake CLI and PTY tests verify argv context, timeout/fallback,
  right requests, status preservation through hooks and no calls while typing.
- The user's `~/.config/starship.toml` was absent on this host; their exact personal prompt
  could not be tested. No personal configuration was created or overwritten.
- Measured 40 samples/case against Bash, Fish and unmodified Release ang.sh. Nash median
  noninteractive `true` startup: 6.52 ms. See BENCHMARKS.md and raw JSON for all results.
  Interactive startup, idle CPU, and memory were not benchmarked.
- GitHub Actions is configured to build, test, sanitize and check formatting; remote results
  are tracked in GitHub Actions, separately from these local results.

## Commits

- `2fc0489`: establish attributed Nash fork; remove bundled build artifacts.
- `e592833`: native Starship, TOML settings, execution/editor changes and regression tests.
- The documentation follow-up containing this report is recorded in Git history.

## Incomplete features and priorities

Nash is early development software. Full POSIX behavior and production readiness are not claimed.
Functions, if/loops/case, subshell language syntax, heredocs, positional script arguments,
advanced parameter/arithmetic/process expansion, trap and pipefail remain unimplemented.
Asynchronous whole and/or lists are not supported. UTF-8 editing is by codepoint rather than
full grapheme cluster; unusual emoji widths vary by terminal. Disk history is append-only,
and multiline history is session-only. Reverse search is substring-based, not fuzzy.

Only Starship has a native adapter. Other named tools are optional external commands with
availability reporting; native Atuin/Direnv/Zoxide/FZF/Prism/yoo adapters and Git-aware completion
are future work. Linux was tested; macOS and other POSIX systems remain planned.

Next priorities: compound-command AST and semantics, broader job/signal/terminal stress testing,
script arguments and expansion conformance, then structured optional-tool adapters and
interactive performance measurements. Do not select Nash as a login shell yet.

## Final job-control follow-up

The final review added per-process stopped-state tracking so a two-stage pipeline consumes
all stop notifications before returning terminal ownership. Both stages can then resume
without a stale stop event returning the prompt early. Completion now excludes filenames
containing terminal control characters. Two PTY cases cover pipeline stop/resume, under
native and Starship prompts, bringing PTY coverage to 21 tests.

After this change, Debug CTest passed all suites in 20.45 s and ASan/UBSan CTest passed all
suites in 14.24 s. Both runs include the unit executable, 25 execution tests and 21 PTY tests.
The Release binary was rebuilt. Benchmarks above were measured before this final
job-control/completion-only follow-up. Formatting and diff-whitespace checks passed.

GitHub Actions passed for both the implementation commit and the first report commit.
The final follow-up's remote workflow is separate and can be inspected on GitHub.

## Prompt default preference update

At the user's subsequent request, Nash now starts with its own native two-line prompt,
including a Nash label, working directory, branch when available and status-aware arrow.
Starship is selected explicitly through `[prompt].engine = "starship"`. Example TOML and
README match the new default. `nash-prompt --engine native|starship` previews an engine.

Debug and Release builds succeeded. Debug CTest passed all three suites in 8.25 s,
including 26 execution tests and 21 PTY tests. Release execution tests also passed (26).
The new regression confirms an inherited Starship environment never activates Starship
when the default native engine is selected. Explicit Starship tests continue to pass.
A fresh PTY session verified the already installed executable displays the native prompt
using the user's new Nash config; its existing Starship config was untouched.
