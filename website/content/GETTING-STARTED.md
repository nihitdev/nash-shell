# Getting started

## Build and install

From the repository, install a C++23 compiler, CMake, toml++ 3 and Python 3 using your package
manager. toml++ is retained for reading older Nash TOML configs; new configs use Nash commands.

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
sudo cmake --install build-release --prefix /usr/local
```

The handbook is installed under `share/doc/nash/docs/` beneath the installation prefix,
including configuration and popular-tool guides.

A previous build configured with `-DBUILD_TESTING=OFF` needs `-DBUILD_TESTING=ON` to run CTest.
For a user-only installation, configure with `-DCMAKE_INSTALL_PREFIX="$HOME/.local"`, rebuild,
then install without sudo. Put that installation's `bin` directory on your PATH.

## Launch and leave

```sh
nash
version
```

Installing does not change the shell in an existing terminal or your account's login shell.
Nash starts with its own native prompt. Type `exit` or press Ctrl-D on an empty input to return.
Keep your current login shell while Nash is under development.

## Make your first config

Create `~/.config/nash/config.nash` with your editor:

```sh
config set prompt.engine native
config set autosuggestions.enabled true
alias ll='ls -lah'
alias gs='git status --short'
# has fastfetch && fastfetch
```

These are Nash commands, not TOML. Save the file and start Nash again. Inside a running Nash,
reload with `source ~/.config/nash/config.nash`. This reruns startup commands, including Fastfetch
if enabled. Hook registrations append; clear a hook first if your config is regularly reloaded.
See [configuration](CONFIGURATION.md) for XDG paths and precedence.

## Run everyday commands

```sh
pwd
cd ~/Projects
ll
printf 'hello\n' | tr a-z A-Z
false
echo $?
echo hello > /tmp/nash-example.txt
cat < /tmp/nash-example.txt
sleep 10
```

During `sleep`, press Ctrl-C to interrupt it. Run `sleep 10 &` to start a background job,
`jobs` to inspect jobs, and `wait` to wait for background work. Ctrl-Z stops foreground work;
`bg` resumes it in the background and `fg` brings it back. Do not assume full Bash job syntax.

## Run a script

Save commands in `hello.nash`:

```sh
NAME=world
echo "Hello $NAME"
```

Run `nash hello.nash`. Ordinary noninteractive runs do not load your personal config.
Script positional arguments, functions, loops and conditionals such as `if` are not supported
currently; `&&` and `||` provide simple success/failure chaining.
