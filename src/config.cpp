#include "config.h"
#include "prompt.h"
#include "shell.h"
#include "util.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <toml++/toml.hpp>
namespace ang {
void apply_defaults(ShellState &st) {
    st.prompt_left = "{blue}{cwd}{reset} {fail}{arrow} ";
    st.prompt_right.clear();
    st.history_file = data_dir() + "/history";
}
void load_config(ShellState &st) {
    auto path = config_dir() + "/config.toml";
    if (std::filesystem::exists(path)) {
        try {
            auto t = toml::parse_file(path);
            st.prompt_engine = t["prompt"]["engine"].value_or(st.prompt_engine);
            if (st.prompt_engine != "starship" && st.prompt_engine != "native") {
                std::cerr << "nash: prompt.engine must be starship or native; using native\n";
                st.prompt_engine = "native";
            }
            auto timeout = t["prompt"]["timeout_ms"].value_or<int64_t>(1500);
            if (timeout < 50 || timeout > 10000)
                throw std::runtime_error("prompt.timeout_ms must be 50..10000");
            st.prompt_timeout_ms = static_cast<int>(timeout);
            if (auto theme = t["prompt"]["theme"].value<std::string>()) {
                std::string error;
                if (!load_theme(*theme, st, &error))
                    std::cerr << "nash: " << error << '\n';
            }
            st.prompt_left = t["prompt"]["left"].value_or(st.prompt_left);
            st.prompt_right = t["prompt"]["right"].value_or(st.prompt_right);
            st.suggestions_enabled = t["autosuggestions"]["enabled"].value_or(true);
            st.highlighting_enabled = t["highlighting"]["enabled"].value_or(true);
            st.completion_enabled = t["completion"]["enabled"].value_or(true);
            st.colors_enabled = t["colors"]["enabled"].value_or(true);
            st.history_enabled = t["history"]["enabled"].value_or(true);
            st.history_file = expand_tilde(t["history"]["path"].value_or(st.history_file));
            auto limit = t["history"]["limit"].value_or<int64_t>(5000);
            if (limit < 1 || limit > 1000000)
                throw std::runtime_error("history.limit must be 1..1000000");
            st.history_limit = static_cast<size_t>(limit);
            st.autocd = t["editor"]["autocd"].value_or(false);
            if (auto keys = t["keybindings"].as_table())
                for (const auto &[key, val] : *keys)
                    if (auto v = val.value<std::string>())
                        st.keybindings[std::string(key.str())] = *v;
            for (const auto *point : {"precmd", "preexec", "chpwd"})
                if (auto commands = t["hooks"][point].as_array())
                    for (const auto &val : *commands)
                        if (auto v = val.value<std::string>())
                            st.hooks[point].push_back(*v);
            st.startup_file = expand_tilde(t["startup"]["file"].value_or(std::string{}));
        } catch (const std::exception &e) {
            std::cerr << "nash: " << path << ": " << e.what() << '\n';
        }
    }
    if (!st.startup_file.empty()) {
        std::ifstream f(st.startup_file);
        if (!f)
            std::cerr << "nash: cannot read startup file " << st.startup_file << '\n';
        else {
            std::ostringstream s;
            s << f.rdbuf();
            run_source(s.str(), st);
        }
    }
    if (st.history_enabled) {
        std::ifstream f(st.history_file);
        std::string line;
        while (std::getline(f, line))
            if (!line.empty())
                st.history.push_back(line);
        if (st.history.size() > st.history_limit)
            st.history.erase(st.history.begin(),
                             st.history.end() - static_cast<std::ptrdiff_t>(st.history_limit));
        st.history_saved = st.history.size();
    }
}
} // namespace ang
