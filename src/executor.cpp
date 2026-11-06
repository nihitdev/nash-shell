// executor.cpp
#include "executor.h"
#include "builtins.h"
#include "expand.h"
#include "integrations.h"
#include "shell.h"
#include "util.h"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <optional>
#include <set>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ang {

bool g_should_exit = false;
int g_exit_code = 0;

// Parent builtins (cd/posix/...) print via iostreams while external commands
// run in forked children. Without a flush, a child would inherit buffered
// parent output and print it a second time at _exit. Flush before every fork.
static void flush_all() {
    std::cout.flush();
    std::cerr.flush();
    fflush(nullptr);
}

// --- interactive pre-processing: history expansion, alias, abbr, autocd ---

std::string preprocess(const std::string &line, ShellState &st, bool *skip) {
    *skip = false;
    std::string s = line;
    if (trim(s).empty()) {
        *skip = true;
        return "";
    }

    // bash-style history expansion: !! and !$ (interactive only)
    if (st.interactive && !st.history.empty()) {
        if (s == "!!")
            s = st.history.back();
        else if (s.size() >= 2 && s.substr(s.size() - 2) == "!!") {
            // trailing !! -> substitute (simple form)
            s = s.substr(0, s.size() - 2) + st.history.back();
        } else {
            size_t p = s.find("!$");
            if (p != std::string::npos) {
                auto w = split_words(st.history.back());
                s.replace(p, 2, w.empty() ? "" : w.back());
            }
        }
    }

    // Expand only complete unquoted command tokens. Byte spans preserve every other
    // character, including quote boundaries, redirects and substitution bodies.
    std::function<std::string(const std::string &, std::set<std::string>, unsigned)> aliases;
    aliases = [&](const std::string &source, std::set<std::string> active, unsigned depth) {
        if (depth > 32)
            throw std::runtime_error("alias expansion limit exceeded");
        std::string error;
        auto tokens = lex(source, &error);
        if (!error.empty())
            return source; // normal lexer path reports the diagnostic
        bool command = true, target = false;
        size_t copied = 0;
        std::string result;
        for (const auto &token : tokens) {
            if (token.kind != TokKind::Word) {
                switch (token.kind) {
                case TokKind::Pipe:
                case TokKind::And:
                case TokKind::Or:
                case TokKind::Semi:
                case TokKind::Bg:
                    command = true;
                    target = false;
                    break;
                default:
                    target = true;
                    break;
                }
                continue;
            }
            if (target) {
                target = false;
                continue;
            }
            if (!command)
                continue;
            if (is_assign_word(token.raw))
                continue;
            command = false;
            if (token.had_quotes || token.raw.find('\\') != std::string::npos)
                continue;
            const auto &name = token.text;
            if (active.contains(name))
                continue;
            const auto alias = st.aliases.find(name);
            const auto abbr = st.abbrs.find(name);
            if (alias == st.aliases.end() && abbr == st.abbrs.end())
                continue;
            auto chain = active;
            chain.insert(name);
            const auto body = alias != st.aliases.end() ? alias->second : abbr->second;
            result += source.substr(copied, token.begin - copied);
            result += aliases(body, std::move(chain), depth + 1);
            copied = token.end;
        }
        result += source.substr(copied);
        return result;
    };
    s = aliases(s, {}, 0);
    return s;
}

// --- redirections ---

static bool apply_redirs(const std::vector<Redir> &rs, std::string *err) {
    for (const auto &r : rs) {
        // POSIX dup targets: `2>&1` (stderr follows stdout), `>&2` (stdout
        // follows stderr). Order matters: `cmd >log 2>&1` sends both to log.
        if (r.target == "&1") {
            if (r.op == "2>" || r.op == "2>>") {
                dup2(STDOUT_FILENO, STDERR_FILENO);
                continue;
            }
            if (r.op == ">" || r.op == ">>")
                continue; // >&1 is a no-op
        }
        if (r.target == "&2") {
            if (r.op == ">" || r.op == ">>") {
                dup2(STDERR_FILENO, STDOUT_FILENO);
                continue;
            }
            if (r.op == "2>" || r.op == "2>>")
                continue; // 2>&2 is a no-op
        }
        int fd = -1;
        if (r.op == "<")
            fd = open(r.target.c_str(), O_RDONLY);
        else if (r.op == ">")
            fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        else if (r.op == ">>")
            fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        else if (r.op == "2>" || r.op == "&>")
            fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
        else if (r.op == "2>>" || r.op == "&>>")
            fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd < 0) {
            if (err)
                *err = "cannot open '" + r.target + "': " + strerror(errno);
            return false;
        }
        bool ok = true;
        if (r.op == "<")
            ok = dup2(fd, STDIN_FILENO) >= 0;
        else if (r.op == "2>" || r.op == "2>>")
            ok = dup2(fd, STDERR_FILENO) >= 0;
        else if (r.op == "&>" || r.op == "&>>")
            ok = dup2(fd, STDOUT_FILENO) >= 0 && dup2(fd, STDERR_FILENO) >= 0;
        else
            ok = dup2(fd, STDOUT_FILENO) >= 0;
        if (!ok) {
            if (err)
                *err = "descriptor duplication failed";
            close(fd);
            return false;
        }

        close(fd);
    }
    return true;
}

// --- pipeline execution ---

namespace {
struct SavedDescriptors {
    std::array<int, 3> saved;
    SavedDescriptors() {
        flush_all();
        for (int i = 0; i < 3; ++i)
            saved[i] = fcntl(i, F_DUPFD_CLOEXEC, 10);
    }
    ~SavedDescriptors() {
        flush_all();
        for (int i = 0; i < 3; ++i) {
            if (saved[i] >= 0) {
                dup2(saved[i], i);
                close(saved[i]);
            } else
                close(i);
        }
    }
};
struct TemporaryEnvironment {
    ShellState &st;
    std::map<std::string, std::optional<std::string>> environment, variables;
    explicit TemporaryEnvironment(ShellState &state) : st(state) {}
    void set(const std::string &name, const std::string &value, bool exported = true) {
        if (!environment.contains(name)) {
            const char *old = getenv(name.c_str());
            environment[name] = old ? std::optional<std::string>(old) : std::nullopt;
            auto it = st.vars.find(name);
            variables[name] = it == st.vars.end() ? std::nullopt : std::optional(it->second);
        }
        st.set_var(name, value, exported);
    }
    ~TemporaryEnvironment() {
        for (const auto &[name, value] : environment) {
            if (value)
                setenv(name.c_str(), value->c_str(), 1);
            else
                unsetenv(name.c_str());
            if (variables[name])
                st.vars[name] = *variables[name];
            else
                st.vars.erase(name);
        }
    }
};
struct PreparedCommand {
    std::vector<std::string> args;
    std::vector<Redir> redirects;
    std::vector<std::pair<std::string, std::string>> assignments;
    std::optional<int> substitution_status;
};
PreparedCommand prepare(const SimpleCmd &cmd, ShellState &st) {
    PreparedCommand result;
    st.substitution_status.reset();
    result.args = expand_args(cmd.args, cmd.quoted, st);
    {
        TemporaryEnvironment assignment_preview(st);
        for (const auto &a : cmd.assigns) {
            std::string name, value;
            is_assign_word(a, &name, &value);
            auto values = expand_word(value, st, false);
            const auto value_expanded = values.empty() ? "" : values.front();
            result.assignments.emplace_back(name, value_expanded);
            assignment_preview.set(name, value_expanded,
                                   !result.args.empty() || getenv(name.c_str()) != nullptr);
        }
    }
    for (const auto &r : cmd.redirs) {
        auto target = r.target;
        if (!r.quoted.raw.empty() || (target != "&1" && target != "&2")) {
            auto expanded = expand_args({target}, {r.quoted}, st);
            if (expanded.size() != 1)
                throw std::runtime_error("ambiguous redirection");
            target = expanded.front();
            // A filename expansion must not turn into a descriptor duplication.
            if (target == "&1" || target == "&2")
                target = "./" + target;
        }
        result.redirects.push_back({r.op, target, r.quoted});
    }
    result.substitution_status = st.substitution_status;
    return result;
}
} // namespace
static int run_pipeline(const Pipeline &pl, ShellState &st) {
    if (!st.posix)
        for (const auto &cmd : pl.cmds)
            if (!cmd.assigns.empty()) {
                std::cerr << "nash: assignment requires --posix mode (or use set NAME value)\n";
                return st.last_status = 2;
            }
    std::vector<PreparedCommand> commands;
    try {
        for (const auto &cmd : pl.cmds)
            commands.push_back(prepare(cmd, st));
    } catch (const std::exception &e) {
        std::cerr << "nash: " << e.what() << '\n';
        return st.last_status = 1;
    }
    if (commands.size() == 1 && !pl.background) {
        auto &cmd = commands.front();
        if (cmd.args.empty() || is_parent_builtin(cmd.args[0])) {
            SavedDescriptors saved;
            std::string error;
            if (!apply_redirs(cmd.redirects, &error)) {
                std::cerr << "nash: " << error << '\n';
                return st.last_status = 1;
            }
            TemporaryEnvironment environment(st);
            for (const auto &[name, value] : cmd.assignments) {
                if (cmd.args.empty())
                    st.set_var(name, value, getenv(name.c_str()) != nullptr);
                else
                    environment.set(name, value);
            }
            bool handled = false;
            int rc = cmd.args.empty() ? cmd.substitution_status.value_or(0)
                                      : run_builtin(cmd.args, st, &handled);
            st.pipeline_status = {rc};
            return st.last_status = rc;
        }
    }
    if (commands.empty())
        return st.last_status = 0;
    ShellState::BgJob job;
    job.cmd = join(pl.cmds[0].args, " ");
    int previous = -1;
    int gate[2];
    if (pipe(gate) < 0) {
        perror("nash: pipe");
        return st.last_status = 1;
    }
    UniqueFd gate_read(gate[0]), gate_write(gate[1]);
    try {
        for (size_t i = 0; i < commands.size(); ++i) {
            bool more = i + 1 < commands.size();
            int fds[2] = {-1, -1};
            if (more && pipe(fds) < 0)
                throw std::runtime_error("pipe failed");
            UniqueFd reader(fds[0]), writer(fds[1]);
            flush_all();
            pid_t pid = fork();
            if (pid < 0)
                throw std::runtime_error("fork failed");
            if (!pid) {
                pid_t group =
                    st.execution_pgid ? st.execution_pgid : (job.pid ? job.pid : getpid());
                setpgid(0, group);
                st.execution_pgid = group;
                for (int sig : {SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGPIPE, SIGCHLD})
                    signal(sig, SIG_DFL);
                close(gate_write.release());
                char c;
                while (read(gate_read.get(), &c, 1) < 0 && errno == EINTR) {
                }
                close(gate_read.release());
                if (previous >= 0) {
                    if (dup2(previous, 0) < 0)
                        _exit(125);
                    close(previous);
                }
                if (more && dup2(writer.get(), 1) < 0)
                    _exit(125);
                if (reader.get() >= 0)
                    close(reader.release());
                if (writer.get() >= 0)
                    close(writer.release());
                st.interactive = false;
                auto &cmd = commands[i];
                for (auto &[name, value] : cmd.assignments)
                    st.set_var(name, value, true);
                std::string error;
                if (!apply_redirs(cmd.redirects, &error)) {
                    std::cerr << "nash: " << error << '\n';
                    flush_all();
                    _exit(1);
                }
                if (cmd.args.empty())
                    _exit(0);
                bool handled = false;
                int rc = run_builtin(cmd.args, st, &handled);
                if (handled) {
                    flush_all();
                    _exit(rc);
                }
                std::vector<char *> argv;
                for (auto &arg : cmd.args)
                    argv.push_back(arg.data());
                argv.push_back(nullptr);
                execvp(argv[0], argv.data());
                int error_number = errno;
                std::cerr << "nash: " << cmd.args[0] << ": " << strerror(error_number) << '\n';
                flush_all();
                _exit(error_number == ENOENT ? 127 : 126);
            }
            if (!job.pid)
                job.pid = st.execution_pgid ? st.execution_pgid : pid;
            setpgid(pid, job.pid);
            job.pids.push_back(pid);
            if (previous >= 0)
                close(previous);
            previous = reader.release();
        }
        if (previous >= 0) {
            close(previous);
            previous = -1;
        }
        if (st.interactive && !pl.background)
            tcsetpgrp(0, job.pid);
        close(gate_write.release());
        if (pl.background) {
            st.last_bg_pid = job.pid;
            st.bg_jobs.push_back(job);
            if (st.interactive)
                std::cerr << '[' << st.bg_jobs.size() << "] " << job.pid << '\n';
            st.pipeline_status = {0};
            return st.last_status = 0;
        }
        int rc = wait_for_job(job, st, true);
        if (job.stopped) {
            st.bg_jobs.push_back(job);
            if (st.interactive)
                std::cerr << '[' << st.bg_jobs.size() << "] stopped " << job.cmd << '\n';
        }
        return st.last_status = rc;
    } catch (const std::exception &e) {
        if (previous >= 0)
            close(previous);
        for (pid_t pid : job.pids)
            kill(pid, SIGKILL);
        close(gate_write.release());
        for (pid_t pid : job.pids) {
            int ws;
            while (waitpid(pid, &ws, 0) < 0 && errno == EINTR) {
            }
        }
        if (st.interactive)
            tcsetpgrp(0, st.shell_pgid);
        std::cerr << "nash: " << e.what() << '\n';
        return st.last_status = 1;
    }
}

int run_jobs(const JobList &jobs, ShellState &st) {
    int last = st.last_status;
    for (const auto &item : jobs.items) {
        for (size_t k = 0; k < item.pipes.size(); ++k) {
            // && / || short-circuit
            if (k > 0) {
                const std::string &op = item.ops[k - 1];
                if (op == "&&" && last != 0)
                    continue;
                if (op == "||" && last == 0)
                    continue;
            }
            last = run_pipeline(item.pipes[k], st);
            if (g_should_exit)
                return g_exit_code;
        }
    }
    return last;
}

} // namespace ang
