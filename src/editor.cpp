#include "editor.h"
#include "env.h"
#include "highlight.h"
#include "integrations.h"
#include "prompt.h"
#include "shell.h"
#include "util.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cwchar>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <poll.h>
#include <set>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
namespace ang {
namespace {
void write_all(const std::string &s) {
    size_t p = 0;
    while (p < s.size()) {
        auto n = write(1, s.data() + p, s.size() - p);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        p += static_cast<size_t>(n);
    }
}
struct RawTerminal {
    termios old{};
    bool active = false;
    RawTerminal() {
        if (tcgetattr(0, &old) < 0)
            throw std::runtime_error("cannot read terminal state");
        auto raw = old;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | IEXTEN | ISIG));
        raw.c_oflag &= static_cast<tcflag_t>(~OPOST);
        raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(0, TCSADRAIN, &raw) < 0)
            throw std::runtime_error("cannot enter terminal editing mode");
        active = true;
        write_all("\033[?2004h");
    }
    ~RawTerminal() {
        if (active) {
            write_all("\033[?2004l");
            tcsetattr(0, TCSADRAIN, &old);
        }
    }
};
size_t previous(const std::string &s, size_t p) {
    if (!p)
        return 0;
    --p;
    while (p && (static_cast<unsigned char>(s[p]) & 0xc0) == 0x80)
        --p;
    return p;
}
size_t next(const std::string &s, size_t p) {
    if (p >= s.size())
        return s.size();
    ++p;
    while (p < s.size() && (static_cast<unsigned char>(s[p]) & 0xc0) == 0x80)
        ++p;
    return p;
}
int width(const std::string &s) { return static_cast<int>(visible_len(s)); }
std::pair<int, int> position(const std::string &s, int col, int columns) {
    int row = 0;
    size_t start = 0;
    do {
        auto end = s.find('\n', start);
        auto part = s.substr(start, end - start);
        col += width(part);
        row += col / columns;
        col %= columns;
        if (end == std::string::npos)
            break;
        ++row;
        col = 0;
        start = end + 1;
    } while (true);
    return {row, col};
}
std::string safe(const std::string &s) {
    std::string out;
    for (unsigned char c : s)
        if (c == '\n' || (c >= 32 && c != 127))
            out += static_cast<char>(c);
    return out;
}
} // namespace
void save_history(ShellState &st) {
    if (!st.history_enabled || st.history_file.empty() || st.history_saved >= st.history.size())
        return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(st.history_file).parent_path(), ec);
    UniqueFd fd(open(st.history_file.c_str(),
                     O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600));
    if (fd.get() < 0 || flock(fd.get(), LOCK_EX) < 0)
        return;
    // Append records while holding a lock; concurrent sessions never overwrite each other.
    for (size_t i = st.history_saved; i < st.history.size(); ++i) {
        if (st.history[i].find('\n') != std::string::npos)
            continue;
        auto record = st.history[i] + "\n";
        size_t p = 0;
        while (p < record.size()) {
            auto n = write(fd.get(), record.data() + p, record.size() - p);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                flock(fd.get(), LOCK_UN);
                return;
            }
            p += static_cast<size_t>(n);
        }
    }
    flock(fd.get(), LOCK_UN);
    st.history_saved = st.history.size();
    if (st.history.size() > st.history_limit) {
        size_t remove = st.history.size() - st.history_limit;
        st.history.erase(st.history.begin(),
                         st.history.begin() + static_cast<std::ptrdiff_t>(remove));
        st.history_saved -= remove;
    }
}
static std::vector<std::string> complete(const std::string &prefix, bool first,
                                         const ShellState &st) {
    std::set<std::string> matches;
    auto scan = [&](const std::filesystem::path &dir, const std::string &base, bool commands) {
        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            auto name = entry.path().filename().string();
            if (std::any_of(name.begin(), name.end(),
                            [](unsigned char c) { return c < 32 || c == 127; }))
                continue;
            if (!name.starts_with(base) || (base.empty() && name.starts_with('.')))
                continue;
            bool directory = entry.is_directory(ec);
            if (commands && (directory || access(entry.path().c_str(), X_OK) != 0))
                continue;
            auto candidate = commands ? name
                                      : (prefix.substr(0, prefix.size() - base.size()) + name +
                                         (directory ? "/" : ""));
            matches.insert(candidate);
        }
    };
    auto slash = prefix.rfind('/');
    auto base = slash == std::string::npos ? prefix : prefix.substr(slash + 1);
    auto directory = slash == std::string::npos ? std::string(".") : prefix.substr(0, slash + 1);
    if (directory.starts_with("~/"))
        directory = st.get("HOME") + directory.substr(1);
    scan(directory, base, false);
    if (first && slash == std::string::npos) {
        for (const auto &name : {"cd", "pwd", "echo", "export", "unset", "alias", "unalias",
                                 "source", "exit", "jobs", "fg", "bg", "wait", "history", "help"})
            if (std::string(name).starts_with(prefix))
                matches.insert(name);
        for (const auto &[name, _] : st.aliases)
            if (name.starts_with(prefix))
                matches.insert(name);
        for (const auto &[name, _] : st.abbrs)
            if (name.starts_with(prefix))
                matches.insert(name);
        std::string path = st.get("PATH");
        size_t p = 0;
        do {
            auto end = path.find(':', p);
            scan(path.substr(p, end - p).empty() ? "." : path.substr(p, end - p), prefix, true);
            if (end == std::string::npos)
                break;
            p = end + 1;
        } while (true);
    }
    return {matches.begin(), matches.end()};
}
static std::optional<std::string> edit_line(ShellState &st, const std::string &left,
                                            const std::string &right) {
    RawTerminal raw;
    std::string buffer, draft, suggestion;
    size_t cursor = 0, history_index = st.history.size();
    int cursor_row = 0;
    auto newline = left.rfind('\n');
    auto tail = newline == std::string::npos ? left : left.substr(newline + 1);
    if (newline != std::string::npos)
        write_all(left.substr(0, newline + 1));

    auto render = [&] {
        winsize ws{};
        ioctl(0, TIOCGWINSZ, &ws);
        int columns = ws.ws_col ? ws.ws_col : 80;
        suggestion.clear();
        if (st.suggestions_enabled && !buffer.empty() && cursor == buffer.size())
            for (auto i = st.history.rbegin(); i != st.history.rend(); ++i)
                if (i->starts_with(buffer) && i->size() > buffer.size()) {
                    suggestion = i->substr(buffer.size());
                    break;
                }
        std::string output = "\r";
        if (cursor_row)
            output += "\033[" + std::to_string(cursor_row) + "A";
        output += "\033[J" + tail;
        bool color = st.colors_enabled && !getenv("NO_COLOR") && st.get("TERM") != "dumb";
        if (color && st.highlighting_enabled)
            output += highlight_line(safe(buffer), st);
        else
            output += safe(buffer);
        if (!suggestion.empty())
            output += (color ? "\033[90m" : "") + safe(suggestion) + (color ? "\033[0m" : "");
        auto end = position(buffer + suggestion, width(tail), columns);
        auto target = position(buffer.substr(0, cursor), width(tail), columns);
        // Explicitly resolve terminal autowrap at the last column.
        if (end.second == 0 && (!buffer.empty() || !suggestion.empty()))
            output += " \b";
        if (end.first > target.first)
            output += "\033[" + std::to_string(end.first - target.first) + "A";
        output += "\r";
        if (target.second)
            output += "\033[" + std::to_string(target.second) + "C";
        if (end.first == 0 && right.find('\n') == std::string::npos && !right.empty() &&
            width(tail) + width(buffer + suggestion) + width(right) + 2 < columns) {
            output +=
                "\033[s\033[" + std::to_string(columns - width(right)) + "G" + right + "\033[u";
        }
        cursor_row = target.first;
        write_all(output);
    };
    render();
    bool paste = false;
    while (true) {
        pollfd fd{0, POLLIN, 0};
        int ready = poll(&fd, 1, 250);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            return std::nullopt;
        }
        if (!ready) {
            reap_jobs(st);
            continue;
        }
        char c;
        if (read(0, &c, 1) != 1)
            return std::nullopt;
        std::string key(1, c);
        if (c == '\033') {
            while (true) {
                pollfd more{0, POLLIN, 0};
                if (poll(&more, 1, 20) <= 0)
                    break;
                char x;
                if (read(0, &x, 1) != 1)
                    break;
                key += x;
                if (key.size() > 2 && ((x >= 'A' && x <= 'Z') || x == '~'))
                    break;
                if (key.size() > 16)
                    break;
            }
        }
        if (key == "\033[200~") {
            paste = true;
            continue;
        }
        if (key == "\033[201~") {
            paste = false;
            render();
            continue;
        }
        auto action = st.keybindings.find(key);
        if (action != st.keybindings.end()) {
            if (action->second == "accept-suggestion")
                key = "\033[C";
            else if (action->second == "history-search")
                key = std::string(1, 18);
            else if (action->second == "beginning-of-line")
                key = std::string(1, 1);
            else if (action->second == "end-of-line")
                key = std::string(1, 5);
        }
        if (!key.empty() && key.size() == 1)
            c = key[0];
        if ((c == '\r' || c == '\n') && !paste && key.size() == 1) {
            cursor = buffer.size();
            suggestion.clear();
            bool old = st.suggestions_enabled;
            st.suggestions_enabled = false;
            render();
            st.suggestions_enabled = old;
            write_all("\r\n");
            return buffer;
        }
        if (c == 3 && key.size() == 1) {
            write_all("^C\r\n");
            st.last_status = 130;
            st.input_cancelled = true;
            return std::string{};
        }
        if (c == 4 && key.size() == 1) {
            if (buffer.empty()) {
                write_all("\r\n");
                return std::nullopt;
            }
            if (cursor < buffer.size())
                buffer.erase(cursor, next(buffer, cursor) - cursor);
        } else if (key == "\033[D" || c == 2)
            cursor = previous(buffer, cursor);
        else if (key == "\033[C" || c == 6) {
            if (cursor < buffer.size())
                cursor = next(buffer, cursor);
            else if (!suggestion.empty()) {
                buffer += suggestion;
                cursor = buffer.size();
            }
        } else if (c == 1 || key == "\033[H")
            cursor = 0;
        else if (c == 5 || key == "\033[F")
            cursor = buffer.size();
        else if (c == 11)
            buffer.erase(cursor);
        else if (c == 21) {
            buffer.erase(0, cursor);
            cursor = 0;
        } else if (c == 23) {
            size_t p = cursor;
            while (p && buffer[p - 1] == ' ')
                --p;
            while (p && buffer[p - 1] != ' ')
                p = previous(buffer, p);
            buffer.erase(p, cursor - p);
            cursor = p;
        } else if (c == 127 || c == 8) {
            if (cursor) {
                auto p = previous(buffer, cursor);
                buffer.erase(p, cursor - p);
                cursor = p;
            }
        } else if (key == "\033[3~") {
            if (cursor < buffer.size())
                buffer.erase(cursor, next(buffer, cursor) - cursor);
        } else if (key == "\033[A" || c == 16) {
            if (history_index == st.history.size())
                draft = buffer;
            if (history_index)
                buffer = st.history[--history_index];
            cursor = buffer.size();
        } else if (key == "\033[B" || c == 14) {
            if (history_index < st.history.size())
                ++history_index;
            buffer = history_index == st.history.size() ? draft : st.history[history_index];
            cursor = buffer.size();
        } else if (c == 18) {
            // Repeated Ctrl-R walks older matches using the original query.
            static std::string query;
            if (history_index == st.history.size())
                query = buffer;
            for (size_t i = history_index; i > 0; --i)
                if (st.history[i - 1].find(query) != std::string::npos) {
                    history_index = i - 1;
                    buffer = st.history[i - 1];
                    cursor = buffer.size();
                    break;
                }
        } else if (c == '\t' && st.completion_enabled && !paste) {
            auto start = buffer.find_last_of(" \t;|&", cursor ? cursor - 1 : 0);
            start = start == std::string::npos ? 0 : start + 1;
            auto prefix = buffer.substr(start, cursor - start);
            if (prefix.find_first_of("'\"\\$") == std::string::npos) {
                auto matches = complete(prefix, start == 0, st);
                if (!matches.empty()) {
                    auto common = matches[0];
                    for (auto &m : matches)
                        while (!m.starts_with(common))
                            common.pop_back();
                    std::string escaped;
                    for (char x : common) {
                        if (std::string(" \t'\"\\$;&|(){}<>*?[]#").find(x) != std::string::npos)
                            escaped += '\\';
                        escaped += x;
                    }
                    if (matches.size() == 1 && !common.ends_with('/'))
                        escaped += ' ';
                    buffer.replace(start, cursor - start, escaped);
                    cursor = start + escaped.size();
                }
            }
        } else if (key == "\033\r" || key == "\033\n") {
            buffer.insert(cursor++, 1, '\n');
        } else if (key.size() == 1 &&
                   (static_cast<unsigned char>(c) >= 32 || (paste && (c == '\n' || c == '\r')))) {
            buffer.insert(cursor++, 1, paste && c == '\r' ? '\n' : c);
        }
        if (!paste)
            render();
    }
}
} // namespace ang

namespace ang {
bool read_line(ShellState &st, const std::string &left, const std::string &right,
               std::string *out) {
    if (!st.interactive)
        return static_cast<bool>(std::getline(std::cin, *out));
    try {
        auto line = edit_line(st, left, right);
        if (!line)
            return false;
        *out = *line;
        return true;
    } catch (const std::exception &e) {
        std::cerr << "nash: editor: " << e.what() << '\n';
        return false;
    }
}
} // namespace ang
