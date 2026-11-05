#include "highlight.h"
#include "lexer.h"
#include "parser.h"
#include "prompt.h"
#include "shell.h"
#include <clocale>
#include <iostream>
#include <stdexcept>
static void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
int main() {
    std::setlocale(LC_ALL, "C.UTF-8");
    std::string err;
    auto tokens = ang::lex("echo 'a b' | cat && echo ok > file", &err);
    require(err.empty(), "lex failed");
    ang::JobList tree;
    require(ang::parse(tokens, true, &tree, &err), "parse failed");
    require(tree.items.size() == 1 && tree.items[0].pipes.size() == 2, "and/or structure");
    require(tree.items[0].pipes[0].cmds.size() == 2, "pipeline structure");
    require(ang::visible_len("\033[31m界e\xcc\x81\033[0m") == 3, "Unicode cell widths");
    require(ang::visible_len("\033]8;;https://example.com\aab\033]8;;\a") == 2, "OSC hyperlinks");
    require(!ang::input_complete("echo 'unfinished"), "quote continuation");
    require(!ang::input_complete("echo hi |"), "pipeline continuation");
    ang::ShellState st;
    require(!ang::highlight_line("echo  'a b'", st).empty(), "highlight rendering");
    std::cout << "unit checks passed\n";
}
