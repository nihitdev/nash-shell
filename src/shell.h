#pragma once
#include "env.h"
namespace ang {
extern bool g_should_exit;
int run_source(const std::string &source, ShellState &st);
// Splits physical input into quote/substitution-aware logical commands.
// Returns false when more input is needed, with a diagnostic in error.
bool input_complete(const std::string &source, std::string *error = nullptr);
void initialize_terminal(ShellState &st);
void reap_jobs(ShellState &st);
int wait_for_job(ShellState::BgJob &job, ShellState &st, bool foreground);
} // namespace ang
