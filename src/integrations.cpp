#include "integrations.h"
#include "shell.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
namespace ang {
UniqueFd::~UniqueFd() {
    if (fd_ >= 0)
        close(fd_);
}
std::optional<std::string> find_executable(const std::string &name) {
    auto valid = [](const std::string &p) {
        std::error_code ec;
        return access(p.c_str(), X_OK) == 0 && !std::filesystem::is_directory(p, ec);
    };
    if (name.find('/') != std::string::npos)
        return valid(name) ? std::optional(name) : std::nullopt;
    std::string path = getenv("PATH") ? getenv("PATH") : "/usr/bin:/bin";
    size_t start = 0;
    do {
        auto end = path.find(':', start);
        auto dir = path.substr(start, end - start);
        auto candidate = (dir.empty() ? "." : dir) + "/" + name;
        if (valid(candidate))
            return candidate;
        if (end == std::string::npos)
            break;
        start = end + 1;
    } while (true);
    return std::nullopt;
}
ToolResult capture_tool(const std::vector<std::string> &args, int timeout_ms) {
    ToolResult result;
    if (args.empty())
        return result;
    int fds[2];
    if (pipe(fds) < 0)
        return result;
    UniqueFd reader(fds[0]), writer(fds[1]);
    fcntl(reader.get(), F_SETFD, FD_CLOEXEC);
    fcntl(writer.get(), F_SETFD, FD_CLOEXEC);
    std::vector<char *> argv;
    for (const auto &arg : args)
        argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);
    std::cout.flush();
    std::cerr.flush();
    fflush(nullptr);
    pid_t pid = fork();
    if (pid < 0)
        return result;
    if (pid == 0) {
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        close(reader.release());
        UniqueFd null(open("/dev/null", O_RDWR | O_CLOEXEC));
        if (null.get() < 0 || dup2(null.get(), 0) < 0 || dup2(null.get(), 2) < 0 ||
            dup2(writer.get(), 1) < 0)
            _exit(125);
        close(writer.release());
        execvp(argv[0], argv.data());
        _exit(errno == ENOENT ? 127 : 126);
    }
    setpgid(pid, pid);
    close(writer.release());
    fcntl(reader.get(), F_SETFL, O_NONBLOCK);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    bool eof = false, done = false;
    int ws = 0;
    while (!eof || !done) {
        char buffer[4096];
        ssize_t n;
        while ((n = read(reader.get(), buffer, sizeof buffer)) > 0) {
            result.output.append(buffer, static_cast<size_t>(n));
            if (result.output.size() > 1024 * 1024)
                break;
        }
        if (n == 0)
            eof = true;
        if (!done) {
            auto w = waitpid(pid, &ws, WNOHANG);
            if (w == pid)
                done = true;
            else if (w < 0 && errno != EINTR) {
                done = true;
                ws = 125 << 8;
            }
        }
        if (eof && done)
            break;
        if (std::chrono::steady_clock::now() >= deadline || result.output.size() > 1024 * 1024) {
            result.timed_out = true;
            kill(-pid, SIGKILL);
            if (!done)
                while (waitpid(pid, &ws, 0) < 0 && errno == EINTR) {
                }
            result.status = 124;
            result.output.clear();
            return result;
        }
        pollfd pfd{reader.get(), POLLIN, 0};
        // A closed stdout may precede process exit; avoid spinning on POLLHUP.
        if (eof)
            poll(nullptr, 0, 5);
        else
            poll(&pfd, 1, 10);
    }
    result.status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 128 + WTERMSIG(ws);
    return result;
}
std::optional<std::string> starship_prompt(const ShellState &st, bool right, int columns) {
    auto executable = find_executable("starship");
    if (!executable)
        return std::nullopt;
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec).string();
    if (ec)
        return std::nullopt;
    // Only pass PWD when it actually names the current directory.
    auto logical = st.get("PWD");
    if (logical.empty() || !std::filesystem::equivalent(logical, cwd, ec))
        logical = cwd;
    size_t jobs = 0;
    for (const auto &job : st.bg_jobs)
        if (!job.reaped)
            ++jobs;
    std::vector<std::string> args{*executable,        "prompt",
                                  "--status",         std::to_string(st.last_status),
                                  "--path",           cwd,
                                  "--logical-path",   logical,
                                  "--terminal-width", std::to_string(columns),
                                  "--cmd-duration",   std::to_string(st.command_duration_ms),
                                  "--jobs",           std::to_string(jobs)};
    if (!st.pipeline_status.empty()) {
        std::string codes;
        for (int code : st.pipeline_status) {
            if (!codes.empty())
                codes += ' ';
            codes += std::to_string(code);
        }
        args.insert(args.end(), {"--pipestatus", codes});
    }
    if (right)
        args.push_back("--right");
    setenv("STARSHIP_SHELL", "nash", 1);
    auto result = capture_tool(args, st.prompt_timeout_ms);
    if (result.status != 0)
        return std::nullopt;
    return result.output;
}
void run_hook(const std::string &point, ShellState &st) {
    if (st.in_hook || !st.hooks.contains(point))
        return;
    const auto commands = st.hooks.at(point);
    int status = st.last_status;
    auto pipeline = st.pipeline_status;
    st.in_hook = true;
    for (const auto &command : commands) {
        run_source(command, st);
        if (g_should_exit)
            break;
    }
    st.in_hook = false;
    st.last_status = status;
    st.pipeline_status = pipeline;
}
void configure_tool_shortcuts(ShellState &st) {
    std::map<std::string, std::string> desired;
    if (st.eza_enabled && find_executable("eza")) {
        desired["ll"] = "eza -lah --icons=auto";
        desired["la"] = "eza -a --icons=auto";
        desired["lt"] = "eza --tree --level=2 --icons=auto";
    }
    if (st.bat_enabled) {
        if (find_executable("bat"))
            desired["view"] = "bat --paging=never";
        else if (find_executable("batcat"))
            desired["view"] = "batcat --paging=never";
    }
    for (const auto &[name, body] : st.managed_tool_aliases)
        if (!desired.contains(name) && st.aliases.contains(name) && st.aliases[name] == body)
            st.aliases.erase(name);
    std::map<std::string, std::string> owned;
    for (const auto &[name, body] : desired) {
        auto previous = st.managed_tool_aliases.find(name);
        if (!st.aliases.contains(name) ||
            (previous != st.managed_tool_aliases.end() && st.aliases[name] == previous->second)) {
            st.aliases[name] = body;
            owned[name] = body;
        }
    }
    st.managed_tool_aliases = std::move(owned);
}
void run_startup_tools(ShellState &st) {
    configure_tool_shortcuts(st);
    if (!st.interactive || !st.fastfetch_enabled || !find_executable("fastfetch"))
        return;
    const int status = st.last_status;
    const auto pipeline = st.pipeline_status;
    run_source("fastfetch", st);
    st.last_status = status;
    st.pipeline_status = pipeline;
}
int bi_integrations(const std::vector<std::string> &, ShellState &st) {
    for (const auto *tool : {"starship", "zoxide", "fzf", "atuin", "direnv", "git", "fastfetch",
                             "eza", "bat", "yazi", "prism", "yoo"}) {
        const std::string name = tool;
        auto path = find_executable(name);
        if (name == "bat" && !path)
            path = find_executable("batcat");
        std::cout << name << ": " << (path ? *path : "not installed");
        if (name == "starship")
            std::cout << " (native prompt integration)";
        else if (name == "fastfetch" || name == "eza" || name == "bat") {
            const bool enabled = name == "fastfetch" ? st.fastfetch_enabled
                                 : name == "eza"     ? st.eza_enabled
                                                     : st.bat_enabled;
            std::cout << " (native " << (name == "fastfetch" ? "startup" : "shortcuts") << "; "
                      << (enabled ? "enabled" : "disabled") << ")";
        } else
            std::cout << " (external command; no native adapter)";
        std::cout << '\n';
    }
    return 0;
}
} // namespace ang
