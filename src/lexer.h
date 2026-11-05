// lexer.h — splits one input line into tokens.
// v0.1 token set is intentionally small: words, pipes, logic ops,
// separators, background, and redirections. No subshells yet.
#pragma once
#include "util.h"
#include <string>
#include <vector>

namespace ang {

enum class TokKind {
    Word,            // command / argument (quotes already stripped)
    Pipe,            // |
    And,             // &&
    Or,              // ||
    Semi,            // ;
    Bg,              // & (background)
    RedirIn,         // <
    RedirOut,        // >
    RedirAppend,     // >>
    RedirErr,        // 2>
    RedirErrAppend,  // 2>>
    RedirBoth,       // &>
    RedirBothAppend, // &>>
    End
};

struct Token {
    TokKind kind = TokKind::End;
    std::string text; // Word text, or operator spelling
    QuoteKind quote_kind =
        QuoteKind::None; // None = unquoted, Single = '...', Double = "..." / $(...)
    std::string raw;
    bool had_quotes = false; // word contained literal quote chars
};

std::vector<Token> lex(const std::string &line, std::string *err = nullptr);

} // namespace ang
