// env.h — shell state: variables, aliases, abbreviations, options.
#pragma once
#include <map>
#include <string>
#include <sys/types.h>
#include <vector>

namespace ang {

struct ShellState {
    // Variables set via `set` (ang) or `export`/assignment (posix).
    // Exported ones are also in the process environment.
    std::map<std::string, std::string> vars;
    std::map<std::string, std::string> aliases;  // bash-style: alias ll='ls -l'
    std::map<std::string, std::string> abbrs;    // fish-style: abbr g='git'
    std::vector<std::string> history;

    bool posix = false;     // false = Ang syntax (default), true = POSIX sh-like
    bool autocd = false;    // zsh-style: bare directory name cds into it
    int last_status = 0;     // $?
    pid_t last_bg_pid = 0;   // $!
    std::string shell_name = "nash";
    std::string theme_name = "default";

    // Prompt templates (see prompt.cpp + themes/*.conf).
    std::string prompt_left;
    std::string prompt_right;

    std::string history_file;
    bool interactive = false;

    // Background jobs started with `&`. Reaped before each prompt (main.cpp)
    // and on demand by the `jobs` builtin.
    struct BgJob {
        pid_t pid = 0;
        std::string cmd;
        bool reaped = false;
        int status = 0;
    };
    std::vector<BgJob> bg_jobs;

    std::string get(const std::string& k, const std::string& dflt = "") const;
    void set_var(const std::string& k, const std::string& v, bool export_env);
    void unset_var(const std::string& k);
};

}  // namespace ang
