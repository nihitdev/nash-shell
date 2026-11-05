// expand.cpp
#include "expand.h"
#include "executor.h" // run_jobs
#include "lexer.h"    // lex
#include "parser.h"   // parse, JobList
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <glob.h>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

namespace ang {

namespace {
int g_cs_depth = 0; // command-substitution recursion guard
}

// Run `inner` as a command (captured stdout) — implements $(...).
// Runs in a forked grandchild so inner state changes never leak out.
static std::string run_captured(const std::string &inner, ShellState &st) {
    if (++g_cs_depth > 8) {
        --g_cs_depth;
        return "";
    }
    std::string out;
    std::string lerr;
    auto toks = lex(inner, &lerr);
    if (lerr.empty() && !toks.empty()) {
        JobList jobs;
        std::string perr;
        if (parse(toks, st.posix, &jobs, &perr)) {
            int pipefd[2] = {-1, -1};
            if (pipe(pipefd) == 0) {
                std::cout.flush();
                std::cerr.flush();
                fflush(nullptr);
                pid_t pid = fork();
                if (pid == 0) {
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                    signal(SIGINT, SIG_DFL);
                    st.interactive = false;
                    st.execution_pgid = getpgrp();
                    st.bg_jobs.clear();
                    int rc = run_jobs(jobs, st);
                    std::cout.flush();
                    std::cerr.flush();
                    fflush(nullptr);
                    _exit(rc);
                }
                close(pipefd[1]);
                char buf[4096];
                ssize_t n = 0;
                while ((n = read(pipefd[0], buf, sizeof buf)) != 0) {
                    if (n < 0) {
                        if (errno == EINTR)
                            continue;
                        break;
                    }
                    out.append(buf, (size_t)n);
                }
                close(pipefd[0]);
                int ws = 0;
                while (waitpid(pid, &ws, 0) < 0 && errno == EINTR) {
                }
            }
        }
    }
    --g_cs_depth;
    // strip trailing newlines, like real shells do
    while (!out.empty() && out.back() == '\n')
        out.pop_back();
    return out;
}

static std::string var_value(const std::string &name, ShellState &st) {
    if (name == "?")
        return std::to_string(st.last_status);
    if (name == "$")
        return std::to_string(getpid());
    if (name == "!")
        return std::to_string(st.last_bg_pid);
    auto it = st.vars.find(name);
    if (it != st.vars.end())
        return it->second;
    const char *e = std::getenv(name.c_str());
    return e ? e : "";
}

std::vector<std::string> expand_word(const std::string &raw, ShellState &st, bool split) {
    struct Field {
        std::string text, pattern;
        bool keep = false;
    };
    std::vector<Field> fields(1);
    char quote = 0;
    auto append = [&](char c, bool literal) {
        fields.back().text += c;
        if (literal && (c == '*' || c == '?' || c == '[' || c == ']' || c == '\\'))
            fields.back().pattern += '\\';
        fields.back().pattern += c;
        fields.back().keep = true;
    };
    auto value = [&](const std::string &v) {
        for (char c : v) {
            if (split && !quote && (c == ' ' || c == '\t' || c == '\n')) {
                if (fields.back().keep)
                    fields.emplace_back();
            } else
                append(c, quote != 0);
        }
    };
    for (size_t i = 0; i < raw.size();) {
        char c = raw[i++];
        if (c == '\'' && quote != '"') {
            quote = quote ? 0 : '\'';
            fields.back().keep = true;
            continue;
        }
        if (c == '"' && quote != '\'') {
            quote = quote ? 0 : '"';
            fields.back().keep = true;
            continue;
        }
        if (c == '\\' && quote != '\'' && i < raw.size()) {
            char next = raw[i];
            if (quote == '"' && std::string("$`\"\\\n").find(next) == std::string::npos)
                append(c, true);
            else {
                ++i;
                if (next != '\n')
                    append(next, true);
            }
            continue;
        }
        if (c == '~' && i == 1 && !quote && (i == raw.size() || raw[i] == '/')) {
            value(home_dir());
            continue;
        }
        if (c != '$' || quote == '\'') {
            append(c, quote != 0);
            continue;
        }
        if (i < raw.size() && raw[i] == '(') {
            size_t begin = ++i;
            int depth = 1;
            char q = 0;
            for (; i < raw.size(); ++i) {
                char x = raw[i];
                if (x == '\\' && q != '\'') {
                    ++i;
                    continue;
                }
                if ((x == '\'' || x == '"') && (!q || q == x)) {
                    q = q ? 0 : x;
                    continue;
                }
                if (!q && x == '(')
                    ++depth;
                if (!q && x == ')' && --depth == 0)
                    break;
            }
            if (depth)
                throw std::runtime_error("unterminated substitution");
            value(run_captured(raw.substr(begin, i - begin), st));
            ++i;
            continue;
        }
        std::string key;
        if (i < raw.size() && raw[i] == '{') {
            auto end = raw.find('}', ++i);
            if (end == std::string::npos)
                throw std::runtime_error("unterminated parameter expansion");
            key = raw.substr(i, end - i);
            i = end + 1;
            auto op = key.find(":-");
            if (op != std::string::npos) {
                auto v = var_value(key.substr(0, op), st);
                value(v.empty() ? key.substr(op + 2) : v);
                continue;
            }
            if (key.find_first_not_of(
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789?@*$#!") !=
                std::string::npos)
                throw std::runtime_error("unsupported parameter expansion");
        } else if (i < raw.size() &&
                   std::string("?$#!@*0123456789").find(raw[i]) != std::string::npos)
            key += raw[i++];
        else {
            while (i < raw.size() &&
                   (std::isalnum(static_cast<unsigned char>(raw[i])) || raw[i] == '_'))
                key += raw[i++];
        }
        if (key.empty())
            append('$', true);
        else
            value(var_value(key, st));
    }
    std::vector<std::string> out;
    for (auto &f : fields) {
        if (!f.keep && split)
            continue;
        if (split && f.pattern.find_first_of("*?[") != std::string::npos) {
            glob_t matches{};
            int result = glob(f.pattern.c_str(), 0, nullptr, &matches);
            if (result == 0)
                for (size_t i = 0; i < matches.gl_pathc; ++i)
                    out.emplace_back(matches.gl_pathv[i]);
            globfree(&matches);
            if (result == 0)
                continue;
            if (result != GLOB_NOMATCH)
                throw std::runtime_error("filename expansion failed");
        }
        out.push_back(f.text);
    }
    return out;
}
std::string expand_one(const std::string &word, ShellState &st, QuoteKind qk) {
    if (qk == QuoteKind::Single)
        return word;
    auto values = expand_word(qk == QuoteKind::Double ? "\"" + word + "\"" : word, st, false);
    return values.empty() ? "" : values.front();
}
std::vector<std::string> expand_args(const std::vector<std::string> &words,
                                     const std::vector<QuoteSpec> &quotes, ShellState &st) {
    std::vector<std::string> out;
    for (size_t i = 0; i < words.size(); ++i) {
        std::vector<std::string> values;
        if (!quotes[i].raw.empty())
            values = expand_word(quotes[i].raw, st, true);
        else
            values = {expand_one(words[i], st, quotes[i].kind)};
        out.insert(out.end(), values.begin(), values.end());
    }
    return out;
}
} // namespace ang
