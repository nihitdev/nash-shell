# Provenance

Nash intentionally improves the existing ang.sh implementation rather than claiming to be an
independent rewrite. Upstream: https://github.com/0x251C08/ang.sh, commit
`fa22add8d9bdf7dd9fec9b29bbc4fb73a76764e7`, MIT, copyright 2026 0x251C08.
Original history and license remain. The original local clone was not modified.

Derived modules: builtins, environment, lexer/parser interfaces and list/pipeline grammar,
executor preprocessing, utility functions, native prompt templates and theme loading.
Nash substantially revises expansion, execution, configuration, terminal editing and highlighting.
New modules: direct tool capture / Starship integration and reusable shell input/job helpers.
New tests, documentation, TOML configuration and project tooling are maintained by nihitdev.

Bundled upstream packaging binaries and nested copies of source were removed from the fork's
working tree. They remain in retained upstream history. No upstream releases are Nash releases;
no Nash binaries or release tags have been published by this work.

toml++ is an external build dependency, licensed MIT. Its headers are not vendored here:
https://github.com/marzer/tomlplusplus. Starship is an optional external executable and is
not redistributed with Nash. Its configuration is read by Starship itself.
