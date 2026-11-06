// Derived lexer interface from ang.sh; Nash preserves source spans and raw quote segments.
#include "lexer.h"
#include <cctype>
#include <stdexcept>
namespace ang {
namespace {
void depth_limit(unsigned depth) {
    if (depth > 128)
        throw std::runtime_error("expansion nesting limit exceeded");
}
size_t escaped(const std::string &s, size_t i) {
    if (i + 1 >= s.size())
        throw std::runtime_error("trailing escape");
    return i + 2;
}
size_t scan_quote(const std::string &s, size_t begin, unsigned depth) {
    depth_limit(depth);
    const char quote = s[begin];
    size_t i = begin + 1;
    while (i < s.size()) {
        if (s[i] == quote)
            return i + 1;
        if (quote == '"' && s[i] == '\\') {
            i = escaped(s, i);
            continue;
        }
        if (quote == '"' && s[i] == '$' && i + 1 < s.size()) {
            if (s[i + 1] == '(') {
                i = scan_command_substitution(s, i, depth + 1);
                continue;
            }
            if (s[i + 1] == '{') {
                i = scan_parameter_expansion(s, i, depth + 1);
                continue;
            }
        }
        ++i;
    }
    throw std::runtime_error("unterminated quote");
}
} // namespace
size_t scan_command_substitution(const std::string &s, size_t dollar, unsigned depth) {
    depth_limit(depth);
    size_t i = dollar + 2;
    unsigned parentheses = 1;
    bool word_start = true;
    while (i < s.size()) {
        if (s[i] == '#' && word_start) {
            while (i < s.size() && s[i] != '\n')
                ++i;
            continue;
        }
        if (s[i] == '\\') {
            const bool continuation = i + 1 < s.size() && s[i + 1] == '\n';
            i = escaped(s, i);
            if (!continuation)
                word_start = false;
            continue;
        }
        if (s[i] == '\'' || s[i] == '"') {
            i = scan_quote(s, i, depth + 1);
            word_start = false;
            continue;
        }
        if (s[i] == '$' && i + 1 < s.size()) {
            if (s[i + 1] == '(') {
                i = scan_command_substitution(s, i, depth + 1);
                word_start = false;
                continue;
            }
            if (s[i + 1] == '{') {
                i = scan_parameter_expansion(s, i, depth + 1);
                word_start = false;
                continue;
            }
        }
        if (s[i] == '(')
            ++parentheses;
        if (s[i] == ')' && --parentheses == 0)
            return i + 1;
        word_start = std::isspace(static_cast<unsigned char>(s[i])) ||
                     std::string(";|&<>()").find(s[i]) != std::string::npos;
        ++i;
    }
    throw std::runtime_error("unterminated command substitution");
}
size_t scan_parameter_expansion(const std::string &s, size_t dollar, unsigned depth) {
    depth_limit(depth);
    size_t i = dollar + 2;
    while (i < s.size()) {
        if (s[i] == '\\') {
            i = escaped(s, i);
            continue;
        }
        if (s[i] == '\'' || s[i] == '"') {
            i = scan_quote(s, i, depth + 1);
            continue;
        }
        if (s[i] == '$' && i + 1 < s.size()) {
            if (s[i + 1] == '{') {
                i = scan_parameter_expansion(s, i, depth + 1);
                continue;
            }
            if (s[i + 1] == '(') {
                i = scan_command_substitution(s, i, depth + 1);
                continue;
            }
        }
        if (s[i] == '}')
            return i + 1;
        ++i;
    }
    throw std::runtime_error("unterminated parameter expansion");
}
std::vector<Token> lex(const std::string &s, std::string *error) {
    if (error)
        error->clear();
    std::vector<Token> out;
    size_t i = 0;
    auto push = [&](TokKind kind, const std::string &text, size_t begin, size_t end) {
        Token token;
        token.kind = kind;
        token.text = text;
        token.begin = begin;
        token.end = end;
        out.push_back(token);
    };
    try {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\r') {
                ++i;
                continue;
            }
            if (c == '#') {
                while (i < s.size() && s[i] != '\n')
                    ++i;
                continue;
            }
            if (c == ';' || c == '\n') {
                push(TokKind::Semi, ";", i, i + 1);
                ++i;
                continue;
            }
            if (c == '|' || c == '&') {
                if (i + 1 < s.size() && s[i + 1] == c) {
                    push(c == '|' ? TokKind::Or : TokKind::And, s.substr(i, 2), i, i + 2);
                    i += 2;
                } else if (c == '&' && i + 1 < s.size() && s[i + 1] == '>') {
                    bool append = i + 2 < s.size() && s[i + 2] == '>';
                    size_t end = i + (append ? 3 : 2);
                    push(append ? TokKind::RedirBothAppend : TokKind::RedirBoth,
                         append ? "&>>" : "&>", i, end);
                    i = end;
                } else {
                    push(c == '|' ? TokKind::Pipe : TokKind::Bg, s.substr(i, 1), i, i + 1);
                    ++i;
                }
                continue;
            }
            bool explicit_fd = std::isdigit(static_cast<unsigned char>(c)) && i + 1 < s.size() &&
                               (s[i + 1] == '>' || s[i + 1] == '<');
            if (c == '<' || c == '>' || explicit_fd) {
                size_t begin = i;
                bool stderr_redir = false;
                if (explicit_fd) {
                    if (!((c == '0' && s[i + 1] == '<') ||
                          ((c == '1' || c == '2') && s[i + 1] == '>')))
                        throw std::runtime_error("unsupported file descriptor redirection");
                    stderr_redir = c == '2';
                    ++i;
                }
                char direction = s[i++];
                bool append = i < s.size() && s[i] == direction;
                if (append) {
                    if (direction == '<')
                        throw std::runtime_error("here documents are not supported");
                    ++i;
                }
                auto kind = direction == '<' ? TokKind::RedirIn
                            : stderr_redir ? (append ? TokKind::RedirErrAppend : TokKind::RedirErr)
                                           : (append ? TokKind::RedirAppend : TokKind::RedirOut);
                push(kind,
                     direction == '<' ? "<"
                     : stderr_redir   ? (append ? "2>>" : "2>")
                                      : (append ? ">>" : ">"),
                     begin, i);
                if (i < s.size() && s[i] == '&') {
                    const size_t dup_begin = i++;
                    const size_t digits = i;
                    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])))
                        ++i;
                    auto fd = s.substr(digits, i - digits);
                    if (direction == '<' || (fd != "1" && fd != "2"))
                        throw std::runtime_error(
                            "only >&1 and >&2 descriptor duplication is supported");
                    push(TokKind::Word, "&" + fd, dup_begin, i);
                }
                continue;
            }
            size_t begin = i;
            while (i < s.size()) {
                char d = s[i];
                if (d == '\\') {
                    i = escaped(s, i);
                    continue;
                }
                if (d == '\'' || d == '"') {
                    i = scan_quote(s, i, 0);
                    continue;
                }
                if (d == '$' && i + 1 < s.size()) {
                    if (s[i + 1] == '(') {
                        i = scan_command_substitution(s, i);
                        continue;
                    }
                    if (s[i + 1] == '{') {
                        i = scan_parameter_expansion(s, i);
                        continue;
                    }
                }
                if (d == '`')
                    throw std::runtime_error("backtick substitution is unsupported; use $(...)");
                if (std::string(" \t\r\n;|&<>").find(d) != std::string::npos)
                    break;
                ++i;
            }
            Token token;
            token.kind = TokKind::Word;
            token.begin = begin;
            token.end = i;
            token.raw = s.substr(begin, i - begin);
            char quote = 0;
            for (size_t j = 0; j < token.raw.size(); ++j) {
                char d = token.raw[j];
                if (d == '\\' && quote != '\'' && j + 1 < token.raw.size()) {
                    token.text += d;
                    token.text += token.raw[++j];
                    continue;
                }
                if (d == '$' && quote != '\'' && j + 1 < token.raw.size() &&
                    (token.raw[j + 1] == '(' || token.raw[j + 1] == '{')) {
                    size_t end = token.raw[j + 1] == '(' ? scan_command_substitution(token.raw, j)
                                                         : scan_parameter_expansion(token.raw, j);
                    token.text += token.raw.substr(j, end - j);
                    j = end - 1;
                    continue;
                }
                if ((d == '\'' || d == '"') && (!quote || quote == d)) {
                    quote = quote ? 0 : d;
                    token.had_quotes = true;
                    token.quote_kind = d == '\'' ? QuoteKind::Single : QuoteKind::Double;
                    continue;
                }
                token.text += d;
            }
            out.push_back(token);
        }
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return {};
    }
    return out;
}
} // namespace ang
