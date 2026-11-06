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

## config.nash and user handbook update

The primary startup file is now `~/.config/nash/config.nash`, containing Nash commands.
The validated `config set/get/hook/unhook/bind/unbind` builtin configures settings at runtime;
`has` guards optional executables. An existing config.nash takes precedence over legacy TOML,
which remains a fallback. A minimal user config.nash was created without deleting the old TOML.

Added a handbook index plus guides for getting started, every supported setting, aliases,
Fastfetch/popular tools, keys, and troubleshooting. Tool recipes link to primary documentation
and distinguish direct CLI usage from native adapters. Examples are tested with isolated tools;
the actual installed tools' help was inspected. No native Direnv/Atuin/FZF adapter is claimed.
The handbook and examples are also included in the install rules for offline reading.

Debug CTest passed all four suites in 24.72 s; ASan/UBSan CTest passed all four in 14.88 s:
unit checks, 29 execution tests, 21 PTY tests, and 3 documentation tests. PTY tests now load
config.nash and verify precedence, aliases, exports and hooks. Documentation tests execute the
shipped config and Fastfetch recipes with fixtures, and validate local Markdown links.
Release rebuilt successfully. Global reinstallation still requires the user's sudo password;
the new executable is directly runnable at `build-release/nash` until then.

## Arch-inspired website and native tool support — 2026-11-06

Added opt-in native Fastfetch startup and managed Eza/Bat shortcuts via
`integrations.fastfetch.enabled`, `integrations.eza.enabled`, and `integrations.bat.enabled`.
Fastfetch runs once per interactive startup and preserves status. Eza supplies ll/la/lt;
Bat supplies view, with batcat fallback. Missing tools are skipped. Custom aliases are preserved,
and disabling an adapter removes only unchanged aliases owned by Nash.

Added the source for a 31-page static documentation website in `website/`: an original
Arch-inspired layout, generated handbook with full-text search and outlines, tool directory
and support filters, native config builder, copy/download controls, mobile navigation,
light/dark themes and an original SVG mark. Existing Markdown remains the maintained handbook;
the website builder retains a self-contained content snapshot for isolated builds.

The user requested manual Vercel deployment. No website deployment was made. ChatGPT hosting
metadata was removed from source; website/vercel.json and website/README.md contain the manual
Vercel root, install/build and output settings. No Vercel project was linked or deployed.

Verification: Debug CTest passed all four suites in 10.07 s; ASan/UBSan CTest passed all four in
13.96 s. Release rebuilt; all 32 execution tests and 3 documentation tests passed. New tests
cover native shortcut setup, custom-alias preservation, disabling shortcuts, batcat fallback,
missing tools and Fastfetch's once-per-startup behavior in native and Starship PTY sessions.
The PTY suite now contains 23 cases. Formatting and diff checks passed.

Website verification: generated and checked 31 HTML pages and 13 search-index targets;
all internal links, anchors and referenced assets passed validation. Headless Chrome checks
passed for desktop/mobile layout, search, tool filters, config generation/download, theme
persistence, navigation, lack of horizontal overflow, and absence of JavaScript page errors.
Desktop/mobile screenshots were visually inspected. The exact manual Vercel build commands
also succeeded in a clean standalone folder using only the retained content snapshot.
GitHub CI now builds and validates the website as well as the shell. Hosting itself remains
for the user to perform; no production-host verification is claimed.

## General command reliability audit — 2026-11-06

The user requested a proactive investigation of command failures. Reproduced supported-command
behavior against clean Bash 5.3.20 and corrected alias tokenization/prefixes/substitutions,
assignment order and status, nested/default/IFS expansion, shared delimiter scanning,
escaped trailing spaces, stable shell identity, shared-group child status ownership, stdio descriptor
prefixes, read buffering/EOF/stdin ownership,
read interruption and flags, exit/cd/type/env/wait validation, and cursor-zero completion.
See RELIABILITY.md for individual failures, causes and fixes. Unsupported language features
remain explicitly unsupported; no full shell-conformance claim is made.

Added a dedicated CTest reliability suite with 15 named tests, including 86 command/expansion
comparisons and 36 read/IFS comparisons against Bash. Added PTY checks for builtin-read
interrupt/recovery, completion at the beginning of input, and interactive invalid-exit handling.
The ordinary execution and documentation suites remain in place.

Debug and Release builds succeeded. Debug CTest passed all five suites in 11.76 s;
ASan/UBSan CTest passed all five in 21.38 s. Release reliability tests (15) and execution tests
(32) passed. The interactive suite contains 29 cases across native and Starship prompts.
Formatting, Python syntax, and diff-whitespace checks passed. The documentation website now
has a reliability-audit page (32 pages total); generated links, anchors, assets and 14 search
entries validate. No hosting deployment command/API was invoked. Global installation requires the
user's sudo password; `build-release/nash` contains the tested fixes until reinstallation.

## Nebula theme and smarter completion — 2026-11-06

Added Nebula, a personal native theme for nihitdev: blue frame, lavender Nash label, icy-blue
working directory, Git branch, status-aware second line and right-side clock. Theme files now
decode newline/backslash escapes. Installed the personal theme under ~/.config/nash/themes and
selected it in the backed-up config.nash. A literal multiline override preserves appearance on
the user's older installed binary until the rebuilt binary is installed.

Extracted completion into a separate module. It handles command positions after operators,
assignment prefixes, directory-only cd, executable paths, quoted/escaped filenames, hidden-file
filtering, tilde paths and variable names. Exact prefixes win, then case-insensitive prefixes,
then fuzzy subsequences. Ambiguous completion displays a compact candidate row; Tab/Shift-Tab
cycle through a cached list. Mid-token replacement preserves following arguments. Git supports
common subcommands and bounded local ref discovery, including simple aliases; switch suggests
local branches while checkout/merge/rebase/show can suggest remote refs and tags. After --,
Git completes paths. No command substitution is evaluated during completion. Directory scans
are bounded at 20000 entries and displayed candidates at 256; Git capture has a 200 ms deadline.

Added isolated completion/theme unit checks and PTY regressions for quoted/fuzzy filenames,
forward/backward cycling, middle-of-line editing and completed Git branch switching under
native and Starship prompts. Git-specific PTY tests skip explicitly if Git is absent.
Updated handbook, README, CI formatting scope and website snapshots. Full arbitrary tool
completion scripts, Git options, descriptions and Fish parity are not claimed. Global installation
still needs the user's sudo password; no login-shell change or hosting deployment was performed.

Verification: final Debug CTest passed all six suites in 17.03 s; final ASan/UBSan CTest passed
all six in 28.25 s. The Release build passed all 33 PTY tests and all 15 reliability tests
(including 122 Bash comparisons). Formatting and whitespace checks passed. Both the current
installed Nash and rebuilt Release binary loaded the personal config and rendered Nebula.
The website build/link/anchor/asset/search checks passed for 32 pages and 14 search entries.

## Gray autosuggestion follow-up — 2026-11-06

The user clarified that the requested feature was gray inline ghost text rather than Tab
completion. Added a separate append-only suggestion provider: recent matching history wins;
after 80 ms of idle typing, unmatched input can predict command, filename, directory, Git
subcommand and variable suffixes using local completion. External Git lookup is disabled on
this path. Predictions are cached for the current input; bare argument separators and leading
private spaces do not generate fallback suggestions. Fuzzy rewrites remain explicit Tab actions.
Right Arrow/Ctrl-F accepts gray text; Enter alone does not accept it.

Added unit regressions for priority, fresh-path predictions, disabled/private suggestions,
variables, escaping and append-only behavior. PTY cases wait for an untyped filename suffix,
inspect gray ANSI styling, accept with Right Arrow, verify Enter does not accept the suffix,
and cancel with Ctrl-C in native and Starship sessions. The color test explicitly unsets
NO_COLOR within its isolated shell; user environment settings are unchanged.

Verification: Debug CTest passed all six suites in 12.54 s; ASan/UBSan passed all six in
24.43 s, followed by both focused gray acceptance/Enter tests after their final assertion
was added. Release passed all 37 PTY tests, all 15 reliability tests and the two focused
gray acceptance/Enter checks. Formatting and whitespace checks passed. Website generation
and link/anchor/asset/search validation passed for 32 pages and 14 search entries. No website
deployment or global binary installation was performed; installation still requires sudo.
