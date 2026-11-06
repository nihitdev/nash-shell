# Editing and keybindings

| Key | Action |
|---|---|
| Tab | complete or extend a shared prefix; repeat to cycle through candidates |
| Shift-Tab | cycle backward through completion candidates |
| Up / Down | navigate session and loaded history |
| Right at end / Ctrl-F | accept the current gray autosuggestion |
| Left / Right | move by UTF-8 codepoint |
| Ctrl-R | search older history by substring; repeat for older matches |
| Ctrl-A / Home | beginning of input |
| Ctrl-E / End | end of input |
| Ctrl-U | delete from start to cursor |
| Ctrl-K | delete from cursor to end |
| Ctrl-W | delete previous whitespace-delimited word |
| Backspace / Delete | remove previous/next codepoint |
| Alt-Enter | insert a newline |
| Ctrl-C | cancel input or interrupt foreground work |
| Ctrl-D | exit on empty input; otherwise delete next codepoint |
| Ctrl-Z | stop foreground external work |

Gray autosuggestions appear inline as you type, without pressing Tab. Recent matching history
has priority. With no history match, an 80 ms typing pause enables append-only predictions from
commands, filenames, directories, Git subcommands and variable names. This fallback uses the
local completion engine with external Git lookup disabled: no program runs on each keystroke.
For example, `cat REA` can show gray `DME.md` even before that command enters history.

Press Right Arrow at the end of input or Ctrl-F to accept the full gray suffix; Enter executes
only the text you have accepted or typed. Editing dismisses/recomputes the prediction. Leading
space suppresses predictions, and `config set autosuggestions.enabled false` disables them.
No arguments are invented after a bare separating space. Ghost text only appends: fuzzy/case
rewrites and open-quote replacements remain explicit Tab operations. New Git branch predictions
require Tab; history can still suggest a previous full Git command.

 Colors distinguish valid commands,
missing commands, quoted strings, operators and variables. A red command can still be a
runtime-expanded command; validation is a simple PATH/builtin/alias check.

Completion recognizes commands after pipes/conditionals, assignment prefixes, aliases,
executable names, file paths and `cd` directories. Exact prefixes rank first; when none match,
case-insensitive prefixes and then fuzzy subsequence matches are tried. Type `cat fzt` to match
`fuzzy-target`, for example. Hidden files require a leading dot. Paths with spaces, quotes and
shell metacharacters are escaped before insertion; open literal quotes are handled safely.
`~/` paths and `$NAME` / `${NAME}` variable-name completion are supported.

Tab inserts a unique candidate or extends a shared prefix, and shows a compact candidate row
when ambiguous. Repeated Tab cycles forward; Shift-Tab cycles backward. Editing or moving the
cursor dismisses the menu. Candidates are capped at 256, and directory enumeration at 20000
entries per request. Continued cycling reuses the candidate list.

For `git`, Tab suggests common subcommands; `switch`, `checkout`, `merge`, `rebase`, and `show`
can complete local/remote branches and tags (`switch` offers local branches). Simple aliases such as `g=git` and `gsw='git switch'`
share this context. Local ref lookup uses direct argv with a 200 ms deadline; it does not fetch
or run shell source. Git options, complex alias bodies, arbitrary tool-specific completions,
command-substitution completion and full variable expansion within paths remain unsupported.
After Git's `--`, completion uses files. Completion never executes substitutions to discover
matches. Filenames containing terminal control characters are excluded.

Cursor movement is by codepoint rather than
full grapheme cluster, so some combined emoji/characters need more than one cursor step.

Customize settings in `config.nash`:

```sh
config set autosuggestions.enabled true
config set highlighting.enabled true
config set completion.enabled true
config bind Ctrl-F accept-suggestion
config bind Ctrl-R history-search
```

The four supported actions are `accept-suggestion`, `history-search`, `beginning-of-line`,
`end-of-line`. `config unbind Ctrl-F` removes an override; it does not disable the default
Ctrl-F behavior. There is no general arbitrary-command keybinding action yet.

Unclosed quotes or trailing `|`, `&&`, `||` request another line with `... `. Ctrl-C abandons
the whole pending command. Bracketed paste buffers pasted newlines until Enter, instead of
executing each pasted line immediately.

Commands beginning with a space are omitted from history. Consecutive duplicates are skipped.
History is appended under a file lock. Multiline commands remain in session history only;
the history limit bounds memory, not disk growth. Reverse search is substring-based, not fuzzy.
