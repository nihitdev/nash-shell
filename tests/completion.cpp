#include "completion.h"
#include "prompt.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
namespace fs = std::filesystem;
static void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main() {
    char dir[] = "/tmp/nash-completion-XXXXXX";
    require(mkdtemp(dir), "temporary directory");
    struct Cleanup {
        fs::path old = fs::current_path(), root;
        ~Cleanup() {
            fs::current_path(old);
            fs::remove_all(root);
        }
    } cleanup{fs::current_path(), dir};
    fs::current_path(dir);
    fs::create_directory("project-folder");
    fs::create_directory(".private");
    fs::create_symlink("missing-target", "broken-link");
    for (auto name : {"project-file", "two words", "it's fine", "fuzzy-target", "UpperName",
                      ".hidden", "dollar$cash"})
        std::ofstream(name) << "fixture";
    std::ofstream("executable") << "#!/bin/sh\n";
    fs::permissions("executable", fs::perms::owner_exec | fs::perms::owner_read);
    ang::ShellState st;
    st.vars["PATH"] = dir;
    st.vars["HOME"] = dir;
    st.vars["NASH_COMPLETION_TEST"] = "value";
    st.aliases["unique_command"] = "echo";
    auto values = [&](const std::string &line) {
        return ang::complete_line(line, line.size(), st).candidates;
    };
    auto contains = [](const auto &v, const std::string &s) {
        return std::find(v.begin(), v.end(), s) != v.end();
    };
    require(values("echo hi | unique_c") == std::vector<std::string>{"unique_command"},
            "command after pipe");
    require(values("echo hi && unique_c") == std::vector<std::string>{"unique_command"},
            "command after conditional");
    require(values("A=1 unique_c") == std::vector<std::string>{"unique_command"},
            "assignment prefix");
    require(values("cd project-") == std::vector<std::string>{"project-folder/"},
            "cd directories only");
    require(values("./ex") == std::vector<std::string>{"./executable"}, "explicit executable path");
    require(values("./project-") == std::vector<std::string>{"./project-folder/"},
            "command path directory");
    require(values("cat broken-") == std::vector<std::string>{"broken-link"},
            "broken symlink completion");
    require(values("cat tw") == std::vector<std::string>{"two\\ words"}, "escaped space");
    require(values("cat \"two w") == std::vector<std::string>{"two\\ words"}, "open double quote");
    require(values("cat 'it") == std::vector<std::string>{"it\\'s\\ fine"}, "open single quote");
    require(values("cat two\\ w") == std::vector<std::string>{"two\\ words"}, "escaped input");
    require(values("cat fzt") == std::vector<std::string>{"fuzzy-target"}, "subsequence match");
    require(values("cat upper") == std::vector<std::string>{"UpperName"},
            "case insensitive fallback");
    require(!contains(values("cat "), ".hidden"), "hidden file filtering");
    require(contains(values("cat .h"), ".hidden"), "explicit hidden file");
    require(values("cat dollar") == std::vector<std::string>{"dollar\\$cash"},
            "literal dollar escaping");
    require(values("echo $NASH_COMPLETION_T") == std::vector<std::string>{"$NASH_COMPLETION_TEST"},
            "variable name");
    require(values("echo ${NASH_COMPLETION_T") ==
                std::vector<std::string>{"${NASH_COMPLETION_TEST}"},
            "braced variable");
    require(values("cat ~/tw") == std::vector<std::string>{"~/two\\ words"}, "home expansion");
    require(values("cat $(touch marker)").empty() && !fs::exists("marker"),
            "never evaluate substitutions");
    require(!values("echo ok > project-").empty(), "redirect completion");
    require(!values("> project-").empty(), "leading redirect without command");
    auto middle = ang::complete_line("cat twoZZ tail", 7, st);
    require(middle.begin == 4 && middle.end == 9 && middle.candidates.front() == "two\\ words",
            "middle token range");
    require(values("git sw") == std::vector<std::string>{"switch"}, "git subcommand");
    st.aliases["g"] = "git";
    require(values("g sw") == std::vector<std::string>{"switch"}, "git alias context");
    std::ofstream("theme.conf") << "prompt_left = \"one\\ntwo\\\\three\"\n";
    std::string error;
    require(ang::load_theme("./theme.conf", st, &error) && st.prompt_left == "one\ntwo\\three",
            "multiline theme decoding");
    std::cout << "completion and theme checks passed\n";
}
