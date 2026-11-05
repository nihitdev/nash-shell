// parser.h — builds a tiny AST over lexer tokens.
//
// Grammar (both modes):
//   list    := andor ((';' | '&') andor?)*
//   andor   := pipeline (('&&' | '||') pipeline)*
//   pipeline:= cmd ('|' cmd)*
//   cmd     := word* redirect*   (+ leading NAME=value assignments)
//
// The parser accepts leading NAME=value in both modes; the *executor*
// rejects it in Ang mode at run time. Deferred rejection means
// `posix on; FOO=bar` works left-to-right in one line, matching user intent.
#pragma once
#include "lexer.h"
#include "util.h"
#include <string>
#include <vector>

namespace ang {

struct Redir {
    std::string op;     // "<", ">", ">>", "2>", "2>>", "&>", "&>>"
    std::string target; // filename
    QuoteSpec quoted;
};

struct SimpleCmd {
    std::vector<std::string> args;
    std::vector<QuoteSpec> quoted; // parallel to args
    std::vector<Redir> redirs;
    std::vector<std::string> assigns; // POSIX leading NAME=value words
};

struct Pipeline {
    std::vector<SimpleCmd> cmds;
    bool background = false;
};

struct AndOr {
    std::vector<Pipeline> pipes;
    std::vector<std::string> ops; // "&&"/"||" between pipes
};

struct JobList {
    std::vector<AndOr> items;
};

bool parse(const std::vector<Token> &toks, bool posix_mode, JobList *out, std::string *err);

} // namespace ang
