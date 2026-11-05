# Packaging

Use CMake install; no prebuilt packages are distributed yet. Example staging:

```sh
DESTDIR="$PWD/stage" cmake --install build --prefix /usr
```

The package must depend on toml++ at build time, a C++23 runtime, and POSIX facilities.
Starship is optional. Preserve LICENSE with both copyright notices. Do not install upstream
ang.sh binaries under the Nash name. Release artifacts must be built and tested from a Nash tag.
