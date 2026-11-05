// Derived lexer interface from ang.sh; Nash preserves raw quote segments.
#include "lexer.h"
#include <functional>
#include <stdexcept>
namespace ang {
std::vector<Token> lex(const std::string &s, std::string *error) {
    if (error)
        error->clear();
    std::vector<Token> out;
    size_t i = 0;
    auto push = [&](TokKind k, const std::string &text) {
        Token t;
        t.kind = k;
        t.text = text;
        out.push_back(t);
    };
    std::function<void(char)> quoted;
    std::function<void()> substitution;
    auto escaped = [&] {
        if (i + 1 >= s.size())
            throw std::runtime_error("trailing escape");
        i += 2;
    };
    substitution = [&] {
        i += 2;
        int depth = 1;
        while (i < s.size()) {
            if (s[i] == '\\') {
                escaped();
                continue;
            }
            if (s[i] == '\'' || s[i] == '"') {
                char q = s[i];
                quoted(q);
                continue;
            }
            if (s[i] == '$' && i + 1 < s.size() && s[i + 1] == '(') {
                substitution();
                continue;
            }
            if (s[i] == '(')
                ++depth;
            if (s[i] == ')' && --depth == 0) {
                ++i;
                return;
            }
            ++i;
        }
        throw std::runtime_error("unterminated command substitution");
    };
    quoted = [&](char q) {
        ++i;
        while (i < s.size()) {
            if (s[i] == q) {
                ++i;
                return;
            }
            if (q == '"' && s[i] == '\\') {
                escaped();
                continue;
            }
            if (q == '"' && s[i] == '$' && i + 1 < s.size() && s[i + 1] == '(') {
                substitution();
                continue;
            }
            ++i;
        }
        throw std::runtime_error("unterminated quote");
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
                push(TokKind::Semi, ";");
                ++i;
                continue;
            }
            if (c == '|' || c == '&') {
                if (i + 1 < s.size() && s[i + 1] == c) {
                    push(c == '|' ? TokKind::Or : TokKind::And, s.substr(i, 2));
                    i += 2;
                } else if (c == '&' && i + 1 < s.size() && s[i + 1] == '>') {
                    bool append = i + 2 < s.size() && s[i + 2] == '>';
                    push(append ? TokKind::RedirBothAppend : TokKind::RedirBoth,
                         append ? "&>>" : "&>");
                    i += append ? 3 : 2;
                } else {
                    push(c == '|' ? TokKind::Pipe : TokKind::Bg, s.substr(i++, 1));
                }
                continue;
            }
            if (c == '<' || c == '>' || (c == '2' && i + 1 < s.size() && s[i + 1] == '>')) {
                bool stderr_redir = c == '2';
                if (stderr_redir)
                    ++i;
                char direction = s[i++];
                bool append = i < s.size() && s[i] == direction;
                if (append) {
                    if (direction == '<')
                        throw std::runtime_error("here documents are not supported");
                    ++i;
                }
                auto k = direction == '<' ? TokKind::RedirIn
                         : stderr_redir   ? (append ? TokKind::RedirErrAppend : TokKind::RedirErr)
                                          : (append ? TokKind::RedirAppend : TokKind::RedirOut);
                push(k, direction == '<' ? "<"
                        : stderr_redir   ? (append ? "2>>" : "2>")
                                         : (append ? ">>" : ">"));
                if (i < s.size() && s[i] == '&') {
                    if (i + 1 >= s.size() || (s[i + 1] != '1' && s[i + 1] != '2'))
                        throw std::runtime_error(
                            "only >&1 and >&2 descriptor duplication is supported");
                    push(TokKind::Word, s.substr(i, 2));
                    i += 2;
                }
                continue;
            }
            size_t start = i;
            while (i < s.size()) {
                char d = s[i];
                if (d == '\\') {
                    escaped();
                    continue;
                }
                if (d == '\'' || d == '"') {
                    quoted(d);
                    continue;
                }
                if (d == '$' && i + 1 < s.size() && s[i + 1] == '(') {
                    substitution();
                    continue;
                }
                if (d == '$' && i + 1 < s.size() && s[i + 1] == '{') {
                    auto end = s.find('}', i + 2);
                    if (end == std::string::npos)
                        throw std::runtime_error("unterminated parameter expansion");
                    i = end + 1;
                    continue;
                }
                if (d == '`')
                    throw std::runtime_error("backtick substitution is unsupported; use $(...)");
                if (std::string(" \t\r\n;|&<>").find(d) != std::string::npos)
                    break;
                ++i;
            }
            Token t;
            t.kind = TokKind::Word;
            t.raw = s.substr(start, i - start);
            char q = 0;
            for (size_t j = 0; j < t.raw.size(); ++j) {
                char d = t.raw[j];
                if (d == '\\' && q != '\'' && j + 1 < t.raw.size()) {
                    t.text += d;
                    t.text += t.raw[++j];
                    continue;
                }
                // Preserve substitutions intact, including their inner quotes.
                if (d == '$' && q != '\'' && j + 1 < t.raw.size() &&
                    (t.raw[j + 1] == '(' || t.raw[j + 1] == '{')) {
                    // t.text is only a compatibility view; expansion uses t.raw.
                    t.text += t.raw.substr(j);
                    break;
                }
                if ((d == '\'' || d == '"') && (!q || q == d)) {
                    q = q ? 0 : d;
                    t.had_quotes = true;
                    t.quote_kind = d == '\'' ? QuoteKind::Single : QuoteKind::Double;
                    continue;
                }
                t.text += d;
            }
            out.push_back(t);
        }
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return {};
    }
    return out;
}
} // namespace ang
