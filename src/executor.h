// executor.h — runs a parsed JobList: pipelines, redirections,
// && / || / ; / & lists. Forks with POSIX fork/execvp.
#pragma once
#include "env.h"
#include "parser.h"

namespace ang {

// Run one full input line's jobs. Returns exit status of last job.
int run_jobs(const JobList &jobs, ShellState &st);

// Used by main for history expansion / alias / abbr pre-processing.
std::string preprocess(const std::string &line, ShellState &st, bool *skip);

} // namespace ang
