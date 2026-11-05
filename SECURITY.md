# Security

Nash is an early development shell. Do not use it as a login shell or to interpret untrusted
scripts until its parsing, signal and descriptor behavior has been audited more widely.

Report vulnerabilities privately through GitHub's private vulnerability reporting when enabled.
If unavailable, request a private reporting channel from the maintainer without disclosing
exploit details publicly. No response-time or support-version guarantee is offered yet.

Configuration, startup files and hooks are trusted executable user input. Nash never sources
Bash/Fish initialization automatically. Config and startup files are not loaded in ordinary
noninteractive runs. Prompt helper output is displayed rather than evaluated. History files
are created with mode 0600, appended under a file lock, and symlink targets are refused.
Commands beginning with a space are not recorded. Avoid typing secrets on command lines.
