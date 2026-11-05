// builtins.h — shell builtins. Pure functions over ShellState so they
// are easy to unit-test and extend.
//
// HOW TO ADD A BUILTIN (for contributors):
//   1. Declare `int bi_<name>(args, st)` here.
//   2. Define it in builtins.cpp.
//   3. Register it in `is_parent_builtin` / `run_builtin` tables.
#pragma once
#include "env.h"
#include <string>
#include <vector>

namespace ang {

// True if the builtin must run in the parent (touches shell state).
bool is_parent_builtin(const std::string &name);

// Run any builtin. Returns exit status. Sets `handled=false` if not one.
int run_builtin(const std::vector<std::string> &args, ShellState &st, bool *handled);

int bi_cd(const std::vector<std::string> &a, ShellState &st);
int bi_exit(const std::vector<std::string> &a, ShellState &st);
int bi_echo(const std::vector<std::string> &a, ShellState &st);
int bi_pwd(const std::vector<std::string> &a, ShellState &st);
int bi_export(const std::vector<std::string> &a, ShellState &st);
int bi_unset(const std::vector<std::string> &a, ShellState &st);
int bi_set(const std::vector<std::string> &a, ShellState &st);
int bi_alias(const std::vector<std::string> &a, ShellState &st);
int bi_unalias(const std::vector<std::string> &a, ShellState &st);
int bi_abbr(const std::vector<std::string> &a, ShellState &st);
int bi_posix(const std::vector<std::string> &a, ShellState &st);
int bi_source(const std::vector<std::string> &a, ShellState &st);
int bi_history(const std::vector<std::string> &a, ShellState &st);
int bi_help(const std::vector<std::string> &a, ShellState &st);
int bi_version(const std::vector<std::string> &a, ShellState &st);
int bi_true(const std::vector<std::string> &a, ShellState &st);
int bi_false(const std::vector<std::string> &a, ShellState &st);
int bi_type(const std::vector<std::string> &a, ShellState &st);
int bi_theme(const std::vector<std::string> &a, ShellState &st);
int bi_env(const std::vector<std::string> &a, ShellState &st);
int bi_read(const std::vector<std::string> &a, ShellState &st);
int bi_jobs(const std::vector<std::string> &a, ShellState &st);
int bi_job_control(const std::vector<std::string> &, ShellState &);
int bi_wait(const std::vector<std::string> &a, ShellState &st);

extern bool g_should_exit;
extern int g_exit_code;

} // namespace ang
