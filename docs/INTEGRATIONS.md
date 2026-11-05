# Optional integrations

Starship is implemented using its direct CLI; see README. Its config is never rewritten.
The direct-capture helper limits output to 1 MiB and kills the helper process group after the
configured timeout. It uses `/dev/null` for stdin/stderr so prompt helpers do not steal input.

`integrations` reports executable discovery; presence is not a claim of a working adapter.
Zoxide, FZF, Atuin, Direnv, Git, Eza, Bat, Yazi, Prism and yoo run as external commands today.
Git prompt modules are implemented by Starship. Native branch/path Git completion is planned.

Lifecycle hooks use arrays of Nash command strings:

```toml
[hooks]
preexec = ["true"]
precmd = ["true"]
chpwd = ["zoxide add ."]
```

The Zoxide example is a user-provided hook, not a native adapter; use it only when installed.
Nash does not evaluate Bash/Fish hook snippets. In particular, Direnv requires validated
structured environment import, Atuin requires a session-aware history adapter, and FZF needs
explicit terminal handoff and selection handling. Those are future work. No exported text
from a tool is automatically evaluated as Nash commands.
