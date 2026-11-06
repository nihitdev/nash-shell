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
    size_t begin = 0, end = 0; // byte range in the original source
    bool had_quotes = false;   // word contained literal quote chars
};

// Shared scanners return the byte after the closing delimiter and reject incomplete input.
size_t scan_command_substitution(const std::string &source, size_t dollar, unsigned depth = 0);
size_t scan_parameter_expansion(const std::string &source, size_t dollar, unsigned depth = 0);
std::vector<Token> lex(const std::string &line, std::string *err = nullptr);

} // namespace ang
