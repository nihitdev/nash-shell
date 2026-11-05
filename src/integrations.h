#pragma once
#include "env.h"
#include <optional>
namespace ang {
class UniqueFd {
    int fd_;

  public:
    explicit UniqueFd(int fd = -1) : fd_(fd) {}
    ~UniqueFd();
    UniqueFd(const UniqueFd &) = delete;
    UniqueFd &operator=(const UniqueFd &) = delete;
    int get() const { return fd_; }
    int release() {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }
};
std::optional<std::string> find_executable(const std::string &name);
struct ToolResult {
    int status = 125;
    std::string output;
    bool timed_out = false;
};
// Direct argv execution: no sh -c, no evaluation of terminal output as shell code.
ToolResult capture_tool(const std::vector<std::string> &args, int timeout_ms);
std::optional<std::string> starship_prompt(const ShellState &st, bool right, int columns);
void run_hook(const std::string &point, ShellState &st);
int bi_integrations(const std::vector<std::string> &, ShellState &);
} // namespace ang
