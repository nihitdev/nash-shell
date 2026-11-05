# Packaging

Use CMake install; no prebuilt packages are distributed yet. Example staging:

```sh
DESTDIR="$PWD/stage" cmake --install build --prefix /usr
```

The package needs toml++ at build time, the matching C++ standard library and POSIX facilities.
When toml++ is linked as a shared library, include its runtime package as a dependency too.
Starship is optional. Preserve LICENSE with both copyright notices. Do not install upstream
ang.sh binaries under the Nash name. Release artifacts must be built and tested from a Nash tag.
