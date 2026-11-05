// env.cpp
#include "env.h"
#include <cstdlib>

namespace ang {

std::string ShellState::get(const std::string &k, const std::string &dflt) const {
    auto it = vars.find(k);
    if (it != vars.end())
        return it->second;
    const char *e = std::getenv(k.c_str());
    return e ? e : dflt;
}

void ShellState::set_var(const std::string &k, const std::string &v, bool export_env) {
    vars[k] = v;
    if (export_env)
        setenv(k.c_str(), v.c_str(), 1);
}

void ShellState::unset_var(const std::string &k) {
    vars.erase(k);
    unsetenv(k.c_str());
}

} // namespace ang
