// parser.cpp
#include "parser.h"
#include "util.h"
#include <set>

namespace ang {

static std::string op_of(TokKind k) {
    switch (k) {
    case TokKind::RedirIn:
        return "<";
    case TokKind::RedirOut:
        return ">";
    case TokKind::RedirAppend:
        return ">>";
    case TokKind::RedirErr:
        return "2>";
    case TokKind::RedirErrAppend:
        return "2>>";
    case TokKind::RedirBoth:
        return "&>";
    case TokKind::RedirBothAppend:
        return "&>>";
    default:
        return "";
    }
}
static bool is_redir(TokKind k) { return !op_of(k).empty(); }

bool parse(const std::vector<Token> &toks, bool posix_mode, JobList *out, std::string *err) {
    (void)posix_mode; // mode is enforced at execution time (see executor)
    out->items.clear();
    size_t i = 0;
    auto peek = [&]() -> TokKind { return i < toks.size() ? toks[i].kind : TokKind::End; };

    // Empty line is fine.
    if (toks.empty())
        return true;

    while (true) {
        while (peek() == TokKind::Semi)
            ++i; // skip stray separators
        if (peek() == TokKind::End)
            break;

        AndOr top;
        bool need_pipe = true;
        std::string pending_op;
        while (need_pipe) {
            Pipeline pl;
            // one pipeline: cmd (| cmd)*
            while (true) {
                if (peek() != TokKind::Word && !is_redir(peek())) {
                    if (err)
                        *err = "expected command, found '" +
                               (i < toks.size() ? toks[i].text : "end") + "'";
                    return false;
                }
                SimpleCmd cmd;
                // collect words + redirs for one simple command
                bool first = true;
                while (peek() == TokKind::Word || is_redir(peek())) {
                    if (is_redir(peek())) {
                        std::string op = op_of(peek());
                        ++i;
                        if (peek() != TokKind::Word) {
                            if (err)
                                *err = "redirect '" + op + "' needs a file";
                            return false;
                        }
                        cmd.redirs.push_back(
                            {op,
                             toks[i].text,
                             {toks[i].quote_kind, toks[i].had_quotes, toks[i].raw}});
                        ++i;
                    } else {
                        std::string w = toks[i].text;
                        static const std::set<std::string> unsupported = {
                            "if",   "then", "else", "elif",     "fi", "for", "while", "until", "do",
                            "done", "case", "esac", "function", "(",  ")",   "{",     "}"};
                        if (first && !toks[i].had_quotes &&
                            (unsupported.contains(w) || w.ends_with("()"))) {
                            if (err)
                                *err = "unsupported compound shell syntax: " + w;
                            return false;
                        }
                        if (first && is_assign_word(toks[i].raw.empty() ? w : toks[i].raw)) {
                            // Accepted in both modes here; Ang-mode rejection
                            // happens at run time (executor) so that
                            // `posix on; FOO=bar` works left-to-right.
                            cmd.assigns.push_back(toks[i].raw.empty() ? w : toks[i].raw);
                            ++i;
                            // still `first` until a non-assign word appears
                        } else {
                            first = false;
                            cmd.args.push_back(w);
                            cmd.quoted.push_back(
                                {toks[i].quote_kind, toks[i].had_quotes, toks[i].raw});
                            ++i;
                        }
                    }
                    if (peek() == TokKind::Pipe)
                        break;
                    if (peek() == TokKind::And || peek() == TokKind::Or ||
                        peek() == TokKind::Semi || peek() == TokKind::Bg || peek() == TokKind::End)
                        break;
                }
                if (cmd.args.empty() && cmd.assigns.empty() && cmd.redirs.empty()) {
                    if (err)
                        *err = "expected command before '|'";
                    return false;
                }
                if (cmd.args.empty() && !cmd.assigns.empty()) {
                    // bare `FOO=bar` with no command: applied by the executor
                    // in POSIX mode, rejected with a hint in Ang mode.
                }
                pl.cmds.push_back(std::move(cmd));
                if (peek() == TokKind::Pipe) {
                    ++i;
                    while (peek() == TokKind::Semi)
                        ++i;
                    continue;
                }
                break;
            }
            top.pipes.push_back(std::move(pl));
            if (peek() == TokKind::And || peek() == TokKind::Or) {
                pending_op = (peek() == TokKind::And) ? "&&" : "||";
                top.ops.push_back(pending_op);
                ++i;
                while (peek() == TokKind::Semi)
                    ++i;
                continue;
            }
            need_pipe = false;
        }
        // background?
        if (!top.pipes.empty() && peek() == TokKind::Bg) {
            top.pipes.back().background = true;
            ++i;
        }
        out->items.push_back(std::move(top));
        if (peek() == TokKind::Semi) {
            ++i;
            continue;
        }
        if (peek() == TokKind::End)
            break;
        // loop continues for `&`-separated lists (Bg consumed above)
    }
    return true;
}

} // namespace ang
