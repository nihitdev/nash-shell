#include "shell.h"
#include "builtins.h"
#include "executor.h"
#include "lexer.h"
#include "parser.h"
#include "util.h"
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
namespace ang {
static int run_chunk(const std::string &, ShellState &);
int run_source(const std::string &raw, ang::ShellState &st) {
    std::string pending;
    size_t position = 0;
    do {
        auto end = raw.find('\n', position);
        pending += raw.substr(position, end == std::string::npos ? end : end - position);
        if (input_complete(pending) || end == std::string::npos) {
            run_chunk(pending, st);
            pending.clear();
            if (g_should_exit)
                break;
        } else
            pending += '\n';
        if (end == std::string::npos)
            break;
        position = end + 1;
    } while (position < raw.size());
    if (!pending.empty() && !g_should_exit)
        run_chunk(pending, st);
    return st.last_status;
}

// Single physical line: expand, lex, parse, run.
int run_chunk(const std::string &raw, ang::ShellState &st) {
    bool skip = false;
    std::string s;
    try {
        s = ang::preprocess(raw, st, &skip);
    } catch (const std::exception &e) {
        std::cerr << "nash: " << e.what() << '\n';
        return st.last_status = 2;
    }
    if (skip)
        return st.last_status;
    if (ang::trim(s).empty())
        return st.last_status;

    // zsh-style autocd: bare directory name cds into it.
    {
        auto toks = ang::lex(s, nullptr);
        if (st.autocd && toks.size() == 1 && toks[0].kind == ang::TokKind::Word) {
            std::string w = ang::expand_tilde(toks[0].text);
            std::error_code ec;
            if (std::filesystem::is_directory(w, ec)) {
                std::vector<std::string> a = {"cd", w};
                int rc = ang::bi_cd(a, st);
                st.last_status = rc;
                return rc;
            }
        }
    }

    std::string lerr;
    auto toks = ang::lex(s, &lerr);
    if (!lerr.empty()) {
        std::cerr << ang::err_prefix() << lerr << "\n";
        st.last_status = 2;
        return 2;
    }
    if (toks.empty())
        return st.last_status;
    ang::JobList jobs;
    std::string perr;
    if (!ang::parse(toks, st.posix, &jobs, &perr)) {
        std::cerr << ang::err_prefix() << perr << "\n";
        st.last_status = 2;
        return 2;
    }
    try {
        return ang::run_jobs(jobs, st);
    } catch (const std::exception &e) {
        std::cerr << "nash: " << e.what() << '\n';
        return st.last_status = 2;
    }
}

bool input_complete(const std::string &source, std::string *error) {
    std::string e;
    auto tokens = lex(source, &e);
    if (error)
        *error = e;
    if (!e.empty())
        return e.find("unterminated") == std::string::npos &&
               e.find("trailing escape") == std::string::npos;
    if (!tokens.empty()) {
        auto k = tokens.back().kind;
        if (k == TokKind::Pipe || k == TokKind::And || k == TokKind::Or)
            return false;
    }
    return true;
}
void initialize_terminal(ShellState &st) {
    if (!st.interactive)
        return;
    while (tcgetpgrp(0) != getpgrp())
        kill(-getpgrp(), SIGTTIN);
    st.shell_pgid = getpid();
    for (int sig : {SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU})
        signal(sig, SIG_IGN);
    if (setpgid(0, st.shell_pgid) < 0 && errno != EACCES && errno != EPERM)
        perror("nash: setpgid");
    tcsetpgrp(0, st.shell_pgid);
    tcgetattr(0, &st.terminal_modes);
}
static int decoded(int ws) {
    return WIFEXITED(ws) ? WEXITSTATUS(ws) : WIFSIGNALED(ws) ? 128 + WTERMSIG(ws) : 128 + SIGTSTP;
}
void reap_jobs(ShellState &st) {
    for (auto &job : st.bg_jobs) {
        if (job.reaped)
            continue;
        for (pid_t pid : job.pids) {
            int ws = 0;
            pid_t result;
            while ((result = waitpid(pid, &ws, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
                if (WIFSTOPPED(ws))
                    job.stopped_pids.insert(pid);
                else if (WIFCONTINUED(ws))
                    job.stopped_pids.erase(pid);
                else {
                    job.results[pid] = ws;
                    job.stopped_pids.erase(pid);
                }
            }
        }
        job.stopped = !job.stopped_pids.empty() &&
                      job.stopped_pids.size() + job.results.size() == job.pids.size();
        job.reaped = job.results.size() == job.pids.size();
        if (job.reaped && !job.pids.empty())
            job.status = decoded(job.results[job.pids.back()]);
    }
}
int wait_for_job(ShellState::BgJob &job, ShellState &st, bool foreground) {
    if (st.interactive && foreground) {
        tcsetpgrp(0, job.pid);
        if (job.has_modes)
            tcsetattr(0, TCSADRAIN, &job.modes);
    }
    while (job.results.size() < job.pids.size() && !job.stopped) {
        int ws = 0;
        pid_t pid = waitpid(-job.pid, &ws, WUNTRACED);
        if (pid < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        // Substitution children may share a process group with background jobs.
        // A group wait can reap those too; never count them as this job's children.
        if (std::find(job.pids.begin(), job.pids.end(), pid) == job.pids.end()) {
            for (auto &background : st.bg_jobs) {
                if (std::find(background.pids.begin(), background.pids.end(), pid) ==
                    background.pids.end())
                    continue;
                if (WIFSTOPPED(ws))
                    background.stopped_pids.insert(pid);
                else {
                    background.results[pid] = ws;
                    background.stopped_pids.erase(pid);
                }
                background.stopped = !background.stopped_pids.empty() &&
                                     background.stopped_pids.size() + background.results.size() ==
                                         background.pids.size();
                background.reaped = background.results.size() == background.pids.size();
                if (background.reaped)
                    background.status = decoded(background.results[background.pids.back()]);
                break;
            }
            continue;
        }
        if (WIFSTOPPED(ws))
            job.stopped_pids.insert(pid);
        else {
            job.results[pid] = ws;
            job.stopped_pids.erase(pid);
        }
        job.stopped = !job.stopped_pids.empty() &&
                      job.stopped_pids.size() + job.results.size() == job.pids.size();
    }
    if (st.interactive && foreground) {
        if (job.stopped) {
            tcgetattr(0, &job.modes);
            job.has_modes = true;
        }
        tcsetpgrp(0, st.shell_pgid);
        tcsetattr(0, TCSADRAIN, &st.terminal_modes);
    }
    st.pipeline_status.clear();
    for (pid_t pid : job.pids)
        st.pipeline_status.push_back(job.results.contains(pid) ? decoded(job.results[pid])
                                                               : 128 + SIGTSTP);
    job.reaped = job.results.size() == job.pids.size();
    return job.status = st.pipeline_status.empty() ? 0 : st.pipeline_status.back();
}
} // namespace ang
