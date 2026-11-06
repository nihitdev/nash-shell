# Command reliability audit

Nash's supported commands were compared with a clean Bash process, with alias expansion enabled
and user startup configuration disabled. Every case runs in an isolated temporary directory.
This is a targeted regression suite for implemented behavior, not a POSIX conformance claim.

## Reproduced failures and fixes

| Area | What broke | Why | Fix |
|---|---|---|---|
| Quoted command names | An alias for `ec` changed `ec"ho" ok` into the wrong command. | Preprocessing matched pieces of a word before seeing its quote boundaries. | Alias matching uses whole lexer tokens and source byte ranges; quoted/escaped command names are preserved. |
| Assignment / redirect prefixes | `X=value shortcut` and `>out shortcut` failed to expand aliases. | The preprocessor treated prefixes as the command word. | Prefix assignments and redirect targets are skipped while finding the actual command. |
| Substitution aliases | An alias inside `$(shortcut)` was reported missing. | Captured source bypassed normal command preprocessing. | Substitution uses the common shell source evaluator in an isolated child. |
| Trailing escapes | A word ending in an escaped space produced a trailing-escape error. | Trimming the source removed a significant escaped character. | Original source whitespace is retained. |
| Assignment order | `A=one B=$A` gave B an empty value. | All right-hand sides were expanded before earlier assignments were visible. | A scoped assignment preview expands values in order and restores the caller's environment. |
| Substitution status | `A=$(false)` incorrectly succeeded. | Captured child status was discarded. | Assignment-only commands return the final substitution's status; a real command still determines its own status. |
| Shared process groups | Background work inside a substitution could make its foreground command report status 148. | A process-group wait counted a different job's child as its own. | Reaped statuses are assigned to the job that owns the PID. |
| Shell identity | `$$` changed inside a command substitution. | The special variable used the forked child PID. | The invoking shell PID is captured once and retained across substitution forks. |
| Nested defaults | `${X:-$Y}` printed a literal `$Y`; quoted defaults split incorrectly. | Fallback words were inserted as plain strings. | Expansion retains quoted fragments until field splitting and globbing, and recursively expands defaults. |
| IFS | Custom field separators were ignored. | Splitting was hardcoded to spaces, tabs and newlines. | Unquoted expansion uses IFS, including empty fields from non-whitespace separators. |
| Nested delimiters | Quoted `)` and substitution comments could close an expansion at the wrong place. | Lexer and expansion had different delimiter scanners. | A bounded shared scanner handles quotes, escapes, parameters, nested substitutions and comments. |
| Standard descriptors | `echo ok 1>out` wrote an extra `1`. | An explicit stdout descriptor was parsed as an argument. | `0<`, `1>`, `1>>`, and supported duplication forms are recognized. Other descriptor operations remain explicitly limited. |
| Redirected read | An earlier EOF could make later reads fail; buffered input crossed descriptor changes. | A shared C++ input stream retained its EOF state and buffered data. | Builtin read consumes fd 0 directly, handles partial EOF, validates names and supports `-r` and IFS splitting. |
| Script stdin | Builtin read could not consume input following its command in a piped script. | Main read all stdin before executing the script. | Stdin scripts consume one logical command at a time, leaving data available for read and external commands. |
| Interrupting read | Ctrl-C during builtin read did not return a usable prompt. | The parent ignored SIGINT and a blocking read kept waiting. | A scoped signal handler interrupts read, restores the shell's handler, and returns status 130. |
| Exit validation | Invalid numeric exit arguments silently returned zero. | `atoi` could not distinguish invalid input. | Numeric parsing checks range and trailing data; statuses are normalized. Too many arguments preserve an interactive shell and fail a noninteractive invocation. |
| cd | Extra arguments were ignored and `cd -- DIR` tried entering `--`. | Options and argument count were not checked. | Explicit option termination, argument checks, and truthful OLDPWD errors. |
| Command lookup | `type missing` incorrectly succeeded. | Unknown names were assumed to be external commands. | PATH lookup verifies the executable; missing names return a failure. |
| env | `env NAME=value COMMAND` printed the environment instead of running the command. | The lightweight builtin ignored arguments. | Argument-bearing env invocations use the system executable. |
| wait | Waiting for an unknown PID succeeded. | The job search returned an initial zero when it found nothing. | Validate requested PID/job IDs and report a non-child failure; multiple requested jobs are handled. |
| Completion | Tab at the beginning of input could select a token after the cursor. | A backwards search used position zero even when no prefix existed. | Cursor-zero completion uses an empty prefix; a PTY regression checks continued editing. |

## Automated coverage

`tests/reliability.py` contains focused regressions plus **86 command/expansion comparisons**
and **36 read/IFS comparisons** against Bash. These compare status and stdout, with negative
checks for diagnostics where appropriate. Bash-only comparisons are explicitly skipped if
Bash is unavailable. Other shell tests continue to cover pipelines, large output, execution,
Starship context, configuration, optional tools, and background work.

PTY cases exercise Ctrl-C during builtin read, recovery afterward, interactive exit validation,
completion from the start of input, and existing foreground/background behavior under native
and Starship prompts. All regression suites run through CTest, including sanitizer builds.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 tests/reliability.py build/nash -v
```

Functions, loops, if/case, heredocs, arbitrary descriptor manipulation, positional script
arguments and other documented language gaps are still unsupported. This audit does not turn
Nash into a drop-in Bash replacement. Additional conformance, process-lifecycle stress and
terminal-emulator coverage remain future work.
