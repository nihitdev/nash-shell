#include "config.h"
#include "integrations.h"
#include "prompt.h"
#include "shell.h"
#include "util.h"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <toml++/toml.hpp>
namespace ang {
void apply_defaults(ShellState &st) {
    st.prompt_left =
        "{bold}{cyan}nash{reset} {blue}{cwd}{reset}{purple}{branch}{reset}\n{fail}{arrow} ";
    st.prompt_right.clear();
    st.history_file = data_dir() + "/history";
}
void load_config(ShellState &st) {
    auto script = config_dir() + "/config.nash";
    auto path = config_dir() + "/config.toml";
    if (std::filesystem::exists(script)) {
        std::ifstream file(script);
        if (!file)
            std::cerr << "nash: cannot read " << script << '\n';
        else {
            std::ostringstream source;
            source << file.rdbuf();
            run_source(source.str(), st);
        }
    } else if (std::filesystem::exists(path)) {
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

int bi_config(const std::vector<std::string> &a, ShellState &st) {
    auto error = [](const std::string &text) {
        std::cerr << "nash: config: " << text << '\n';
        return 2;
    };
    if (a.size() == 1 || a[1] == "--help") {
        std::cout << "config set KEY VALUE | config get KEY\n"
                     "config hook precmd|preexec|chpwd 'COMMAND' | config unhook POINT\n"
                     "config bind KEY ACTION | config unbind KEY\n"
                     "Startup file: "
                  << config_dir() << "/config.nash\n";
        return 0;
    }
    std::map<std::string, bool *> flags = {
        {"autosuggestions.enabled", &st.suggestions_enabled},
        {"highlighting.enabled", &st.highlighting_enabled},
        {"completion.enabled", &st.completion_enabled},
        {"history.enabled", &st.history_enabled},
        {"colors.enabled", &st.colors_enabled},
        {"integrations.fastfetch.enabled", &st.fastfetch_enabled},
        {"integrations.eza.enabled", &st.eza_enabled},
        {"integrations.bat.enabled", &st.bat_enabled},
        {"editor.autocd", &st.autocd}};
    std::map<std::string, std::string *> strings = {
        {"prompt.engine", &st.prompt_engine}, {"prompt.theme", &st.theme_name},
        {"prompt.left", &st.prompt_left},     {"prompt.right", &st.prompt_right},
        {"history.path", &st.history_file},   {"startup.file", &st.startup_file}};
    const auto &op = a[1];
    if (op == "get") {
        if (a.size() != 3)
            return error("usage: config get KEY");
        if (flags.contains(a[2]))
            std::cout << (*flags[a[2]] ? "true" : "false") << '\n';
        else if (strings.contains(a[2]))
            std::cout << *strings[a[2]] << '\n';
        else if (a[2] == "prompt.timeout_ms")
            std::cout << st.prompt_timeout_ms << '\n';
        else if (a[2] == "history.limit")
            std::cout << st.history_limit << '\n';
        else
            return error("unknown setting: " + a[2]);
        return 0;
    }
    if (op == "set") {
        if (a.size() != 4)
            return error("usage: config set KEY VALUE (quote values containing spaces)");
        const auto &key = a[2];
        const auto &value = a[3];
        if (flags.contains(key)) {
            if (value != "true" && value != "false" && value != "on" && value != "off")
                return error(key + " expects true or false");
            *flags[key] = value == "true" || value == "on";
            if (key.starts_with("integrations."))
                configure_tool_shortcuts(st);
        } else if (key == "prompt.theme") {
            std::string detail;
            if (!load_theme(value, st, &detail))
                return error(detail);
        } else if (strings.contains(key)) {
            if (key == "prompt.engine" && value != "native" && value != "starship")
                return error("prompt.engine expects native or starship");
            *strings[key] =
                (key == "history.path" || key == "startup.file") ? expand_tilde(value) : value;
        } else if (key == "history.limit" || key == "prompt.timeout_ms") {
            long long number = 0;
            auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), number);
            long long minimum = key == "history.limit" ? 1 : 50,
                      maximum = key == "history.limit" ? 1000000 : 10000;
            if (ec != std::errc{} || end != value.data() + value.size() || number < minimum ||
                number > maximum)
                return error(key + " must be " + std::to_string(minimum) + ".." +
                             std::to_string(maximum));
            if (key == "history.limit")
                st.history_limit = static_cast<size_t>(number);
            else
                st.prompt_timeout_ms = static_cast<int>(number);
        } else
            return error("unknown setting: " + key);
        return 0;
    }
    if (op == "hook" || op == "unhook") {
        if (a.size() != (op == "hook" ? 4U : 3U))
            return error("usage: config hook POINT 'COMMAND' | config unhook POINT");
        if (a[2] != "precmd" && a[2] != "preexec" && a[2] != "chpwd")
            return error("unknown hook: " + a[2]);
        if (op == "hook")
            st.hooks[a[2]].push_back(a[3]);
        else
            st.hooks.erase(a[2]);
        return 0;
    }
    if (op == "bind" || op == "unbind") {
        if (a.size() != (op == "bind" ? 4U : 3U))
            return error("usage: config bind KEY ACTION | config unbind KEY");
        static const std::map<std::string, std::string> keys = {{"Ctrl-A", std::string(1, 1)},
                                                                {"Ctrl-E", std::string(1, 5)},
                                                                {"Ctrl-F", std::string(1, 6)},
                                                                {"Ctrl-R", std::string(1, 18)},
                                                                {"Right", "\033[C"}};
        auto key = keys.contains(a[2]) ? keys.at(a[2]) : a[2];
        if (op == "unbind") {
            st.keybindings.erase(key);
            return 0;
        }
        if (a[3] != "accept-suggestion" && a[3] != "history-search" &&
            a[3] != "beginning-of-line" && a[3] != "end-of-line")
            return error("unsupported keybinding action: " + a[3]);
        st.keybindings[key] = a[3];
        return 0;
    }
    return error("unknown operation: " + op);
}
} // namespace ang
