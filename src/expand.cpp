// Quote-aware expansion: preserve literal/quoted fragments until splitting and globbing.
#include "expand.h"
#include "integrations.h"
#include "lexer.h"
#include "shell.h"
#include <algorithm>
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
// Keep $$ stable across forked substitutions, as an identity of the invoking shell.
const pid_t shell_identity = getpid();
int substitution_depth = 0;
struct SubstitutionDepth {
    SubstitutionDepth() {
        if (substitution_depth >= 8)
            throw std::runtime_error("command substitution nesting limit exceeded");
        ++substitution_depth;
    }
    ~SubstitutionDepth() { --substitution_depth; }
};
std::string run_captured(const std::string &source, ShellState &st) {
    SubstitutionDepth depth;
    int pipes[2];
    if (pipe(pipes) < 0)
        throw std::runtime_error("command substitution: pipe failed");
    UniqueFd reader(pipes[0]), writer(pipes[1]);
    std::cout.flush();
    std::cerr.flush();
    fflush(nullptr);
    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("command substitution: fork failed");
    if (!pid) {
        close(reader.release());
        if (dup2(writer.get(), STDOUT_FILENO) < 0)
            _exit(125);
        close(writer.release());
        for (int sig : {SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGPIPE, SIGCHLD})
            signal(sig, SIG_DFL);
        st.interactive = false;
        st.execution_pgid = getpgrp();
        st.bg_jobs.clear();
        const int rc = run_source(source, st);
        std::cout.flush();
        std::cerr.flush();
        fflush(nullptr);
        _exit(rc);
    }
    close(writer.release());
    std::string output;
    char buffer[4096];
    while (true) {
        const auto count = read(reader.get(), buffer, sizeof(buffer));
        if (count == 0)
            break;
        if (count < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        output.append(buffer, static_cast<size_t>(count));
    }
    int ws = 0;
    pid_t result;
    do {
        result = waitpid(pid, &ws, 0);
    } while (result < 0 && errno == EINTR);
    if (result < 0)
        throw std::runtime_error("command substitution: wait failed");
    st.substitution_status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 128 + WTERMSIG(ws);
    if (output.find('\0') != std::string::npos) {
        std::erase(output, '\0');
        std::cerr << "nash: command substitution: ignored null byte in output\n";
    }
    while (!output.empty() && output.back() == '\n')
        output.pop_back();
    return output;
}
std::string variable(const std::string &name, const ShellState &st) {
    if (name == "?")
        return std::to_string(st.last_status);
    if (name == "$")
        return std::to_string(shell_identity);
    if (name == "!")
        return st.last_bg_pid ? std::to_string(st.last_bg_pid) : "";
    return st.get(name);
}
struct Atom {
    char c = 0;
    bool quoted = false;
    bool split = false;
    bool empty = false;
};
using Atoms = std::vector<Atom>;
Atoms expand_atoms(const std::string &raw, ShellState &st, bool inherited_quote = false,
                   unsigned depth = 0) {
    if (depth > 64)
        throw std::runtime_error("parameter expansion nesting limit exceeded");
    Atoms out;
    char quote = 0;
    auto append = [&](char c, bool quoted, bool split = false) {
        out.push_back({c, quoted, split, false});
    };
    auto value = [&](const std::string &text, bool quoted) {
        for (char c : text)
            append(c, quoted, true);
    };
    for (size_t i = 0; i < raw.size();) {
        const char c = raw[i++];
        const bool quoted = inherited_quote || quote != 0;
        if (c == '\'' && quote != '"' && !inherited_quote) {
            quote = quote ? 0 : '\'';
            out.push_back({0, true, false, true});
            continue;
        }
        if (c == '"' && quote != '\'') {
            quote = quote ? 0 : '"';
            out.push_back({0, true, false, true});
            continue;
        }
        if (c == '\\' && quote != '\'' && i < raw.size()) {
            const char next = raw[i];
            if ((quote == '"' || inherited_quote) &&
                std::string("$`\"\\\n").find(next) == std::string::npos)
                append(c, true);
            else {
                ++i;
                if (next != '\n')
                    append(next, true);
            }
            continue;
        }
        if (c == '~' && i == 1 && !quoted && (i == raw.size() || raw[i] == '/')) {
            value(home_dir(), true);
            continue;
        }
        if (c == '`' && quote != '\'')
            throw std::runtime_error("backtick substitution is unsupported; use $(...)");
        if (c != '$' || quote == '\'') {
            append(c, quoted);
            continue;
        }
        if (i < raw.size() && raw[i] == '(') {
            const auto end = scan_command_substitution(raw, i - 1);
            value(run_captured(raw.substr(i + 1, end - i - 2), st), quoted);
            i = end;
            continue;
        }
        if (i < raw.size() && raw[i] == '{') {
            const auto end = scan_parameter_expansion(raw, i - 1);
            const auto expression = raw.substr(i + 1, end - i - 2);
            const auto op = expression.find(":-");
            const auto name = expression.substr(0, op);
            if (name.empty() ||
                name.find_first_not_of(
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789?@*$#!") !=
                    std::string::npos)
                throw std::runtime_error("unsupported parameter expansion");
            const auto existing = variable(name, st);
            if (op != std::string::npos && existing.empty()) {
                auto fallback = expand_atoms(expression.substr(op + 2), st, quoted, depth + 1);
                for (auto atom : fallback) {
                    atom.quoted = atom.quoted || quoted;
                    atom.split = true;
                    out.push_back(atom);
                }
            } else
                value(existing, quoted);
            i = end;
            continue;
        }
        std::string name;
        if (i < raw.size() && std::string("?$#!@*0123456789").find(raw[i]) != std::string::npos)
            name += raw[i++];
        else
            while (i < raw.size() &&
                   (std::isalnum(static_cast<unsigned char>(raw[i])) || raw[i] == '_'))
                name += raw[i++];
        if (name.empty())
            append('$', quoted);
        else
            value(variable(name, st), quoted);
    }
    if (quote)
        throw std::runtime_error("unterminated quote in expansion");
    return out;
}
} // namespace
std::vector<std::string> expand_word(const std::string &raw, ShellState &st, bool split) {
    struct Field {
        std::string text, pattern;
        bool keep = false;
    };
    std::vector<Field> fields(1);
    const auto ifs = st.get("IFS", " \t\n");
    bool after_whitespace = false;
    for (const auto &atom : expand_atoms(raw, st)) {
        if (atom.empty) {
            fields.back().keep = true;
            after_whitespace = false;
            continue;
        }
        if (split && atom.split && !atom.quoted && ifs.find(atom.c) != std::string::npos) {
            const bool whitespace = atom.c == ' ' || atom.c == '\t' || atom.c == '\n';
            if (whitespace) {
                if (fields.back().keep) {
                    fields.emplace_back();
                    after_whitespace = true;
                }
            } else {
                if (!after_whitespace) {
                    fields.back().keep = true;
                    fields.emplace_back();
                }
                after_whitespace = false;
            }
            continue;
        }
        after_whitespace = false;
        auto &field = fields.back();
        field.keep = true;
        field.text += atom.c;
        if (atom.quoted && std::string("*?[]\\").find(atom.c) != std::string::npos)
            field.pattern += '\\';
        field.pattern += atom.c;
    }
    std::vector<std::string> out;
    for (const auto &field : fields) {
        if (!field.keep && split)
            continue;
        if (split && field.pattern.find_first_of("*?[") != std::string::npos) {
            glob_t matches{};
            const int rc = glob(field.pattern.c_str(), 0, nullptr, &matches);
            if (rc == 0)
                for (size_t i = 0; i < matches.gl_pathc; ++i)
                    out.emplace_back(matches.gl_pathv[i]);
            globfree(&matches);
            if (rc == 0)
                continue;
            if (rc != GLOB_NOMATCH)
                throw std::runtime_error("filename expansion failed");
        }
        out.push_back(field.text);
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
