# Editing and keybindings

| Key | Action |
|---|---|
| Tab | complete a command/path; extend the shared prefix when ambiguous |
| Up / Down | navigate session and loaded history |
| Right at end / Ctrl-F | accept the current history suggestion |
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

History suggestions appear after a matching prefix. Colors distinguish valid commands,
missing commands, quoted strings, operators and variables. A red command can still be a
runtime-expanded command; validation is a simple PATH/builtin/alias check.

Completion handles executable names and file/directory paths. It is not yet fuzzy or Git-aware.
Quoted/complex expansion prefixes have limited completion. Filenames containing terminal
control characters are excluded from suggestions. Cursor movement is by codepoint rather than
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
