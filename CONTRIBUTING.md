# Contributing

Build with CMake and C++23; toml++ 3 and Python 3 are required. Run `ctest --test-dir build
--output-on-failure` before submitting. Keep changes small, preserve upstream attribution,
and document shell semantics and unsupported cases. Never claim POSIX compliance without
a conformance suite. New parser/executor behavior needs black-box regression tests; terminal
changes need PTY tests. Never run test startup files from your personal configuration.

Format C++ using `clang-format -i src/*.{cpp,h} tests/unit.cpp`. Static analysis configuration
is in `.clang-tidy`; use `clang-tidy -p build src/<file>.cpp` with a compiler-compatible version.
For sanitizers configure with `-DNASH_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug`.

Do not commit generated binaries, package caches, secrets, personal prompt configs or build trees.
Native adapters must have tests and explicit hook semantics. Updates to inherited code remain
covered by the existing MIT license notice. Use GitHub issues for reproducible non-sensitive bugs.
