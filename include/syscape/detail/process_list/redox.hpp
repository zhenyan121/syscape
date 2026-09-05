#ifndef SYSCAPE_DETAIL_PROCESS_LIST_REDOX_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_REDOX_HPP

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/process_list/common.hpp>
#include <syscape/process_list.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

inline bool is_space(char c) noexcept {
    return c == ' ' || c == '\t';
}

inline bool parse_uint(std::string_view s, std::uint32_t& val) noexcept {
    if (s.empty()) {
        return false;
    }
    std::uint32_t value = 0U;
    for (char c : s) {
        if (!is_digit(c)) {
            return false;
        }
        const auto digit = static_cast<std::uint32_t>(c - '0');
        if (value >
            ((std::numeric_limits<std::uint32_t>::max)() - digit) / 10U) {
            return false;
        }
        value = value * 10U + digit;
    }
    val = value;
    return true;
}

inline bool parse_pid(std::string_view s, std::uint32_t& pid) noexcept {
    return parse_uint(s, pid);
}

inline bool is_valid_memory_val(std::string_view s) noexcept {
    if (s.empty()) {
        return false;
    }
    bool saw_dot = false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '.') {
            if (saw_dot || i == 0 || i + 1 == s.size()) {
                return false;
            }
            saw_dot = true;
        } else if (!is_digit(c)) {
            return false;
        }
    }
    return true;
}

inline bool is_valid_memory_unit(std::string_view s) noexcept {
    return s == "B" || s == "KB" || s == "MB" || s == "GB" || s == "TB" ||
           s == "PB";
}

inline bool parse_memory_field(std::string_view line, std::size_t& pos,
                               std::string_view& val, std::string_view& unit,
                               bool is_private) noexcept {
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        ++pos;
    }
    if (pos >= line.size()) {
        return false;
    }
    const std::size_t val_start = pos;
    while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t') {
        ++pos;
    }
    val = line.substr(val_start, pos - val_start);
    if (!is_valid_memory_val(val)) {
        return false;
    }
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        ++pos;
    }
    if (pos >= line.size()) {
        return false;
    }
    const char c = line[pos];
    if (c == 'B') {
        unit = line.substr(pos, 1U);
        ++pos;
    } else if (c == 'K' || c == 'M' || c == 'G' || c == 'T' || c == 'P') {
        if (pos + 1U >= line.size() || line[pos + 1U] != 'B') {
            return false;
        }
        unit = line.substr(pos, 2U);
        pos += 2U;
    } else {
        return false;
    }
    if (is_private) {
        if (pos < line.size()) {
            const char next_c = line[pos];
            if (!is_space(next_c) && !is_digit(next_c)) {
                return false;
            }
        }
    }
    return true;
}

inline int process_state_priority(process_list::process_state s) noexcept {
    switch (s) {
    case process_list::process_state::running:
        return 4;
    case process_list::process_state::sleeping:
        return 3;
    case process_list::process_state::stopped:
        return 2;
    case process_list::process_state::unknown:
        return 1;
    case process_list::process_state::zombie:
        return 0;
    }
    return 0;
}

inline result<std::vector<process_list::process_entry>>
parse_sys_context(std::string_view content) {
    std::vector<process_list::process_entry> raw_entries;
    std::string_view remaining = content;
    bool saw_header = false;

    auto is_header_line = [](std::string_view l) noexcept {
        std::size_t p = 0U;
        auto next_tok = [&]() -> std::string_view {
            while (p < l.size() && is_space(l[p])) {
                ++p;
            }
            if (p >= l.size()) {
                return {};
            }
            const std::size_t s = p;
            while (p < l.size() && !is_space(l[p])) {
                ++p;
            }
            return l.substr(s, p - s);
        };
        return next_tok() == "PID" && next_tok() == "EUID" &&
               next_tok() == "EGID" && next_tok() == "STAT" &&
               next_tok() == "CPU" && next_tok() == "AFFINITY" &&
               next_tok() == "TIME" && next_tok() == "PRIVATE" &&
               next_tok() == "SHARED" && next_tok() == "NAME" &&
               next_tok().empty();
    };

    while (!remaining.empty()) {
        const std::size_t newline = remaining.find('\n');
        std::string_view line = newline != std::string_view::npos
                                    ? remaining.substr(0U, newline)
                                    : remaining;
        remaining = newline != std::string_view::npos
                        ? remaining.substr(newline + 1U)
                        : std::string_view();

        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' ||
                                 line.back() == '\t')) {
            line.remove_suffix(1U);
        }
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line.remove_prefix(1U);
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }

        if (!saw_header) {
            if (!is_header_line(line)) {
                return fail(errc::malformed_data);
            }
            saw_header = true;
            continue;
        }

        std::size_t pos = 0U;
        auto skip_spaces = [&]() {
            while (pos < line.size() && is_space(line[pos])) {
                ++pos;
            }
        };
        auto read_token = [&]() -> std::string_view {
            skip_spaces();
            if (pos >= line.size()) {
                return {};
            }
            const std::size_t start = pos;
            while (pos < line.size() && !is_space(line[pos])) {
                ++pos;
            }
            return line.substr(start, pos - start);
        };

        const std::string_view pid_tok = read_token();
        std::uint32_t pid = 0U;
        if (!parse_pid(pid_tok, pid)) {
            return fail(errc::malformed_data);
        }

        // EUID and EGID are effective IDs in Redox /scheme/sys/context.
        // Validate them to detect corruption, but do not populate real UID/GID.
        const std::string_view euid_tok = read_token();
        std::uint32_t euid = 0U;
        if (!parse_uint(euid_tok, euid)) {
            return fail(errc::malformed_data);
        }

        const std::string_view egid_tok = read_token();
        std::uint32_t egid = 0U;
        if (!parse_uint(egid_tok, egid)) {
            return fail(errc::malformed_data);
        }

        const std::string_view stat_tok = read_token();
        if (stat_tok.size() < 2U || stat_tok.size() > 3U) {
            return fail(errc::malformed_data);
        }
        const char mem_cat = stat_tok[0];
        if (mem_cat != 'K' && mem_cat != 'U' && mem_cat != 'R') {
            return fail(errc::malformed_data);
        }
        const char sched_stat = stat_tok[1];
        if (sched_stat != 'R' && sched_stat != 'S' && sched_stat != 'B' &&
            sched_stat != 'Z') {
            return fail(errc::malformed_data);
        }
        if (stat_tok.size() == 3U && stat_tok[2] != '+') {
            return fail(errc::malformed_data);
        }

        // In Redox context.rs: stat_string[0] is memory category ('K'/'U'/'R'),
        // where 'R' means raw (no address space). stat_string[1] is scheduling
        // status ('R' runnable, 'S'/'B' blocked/sleeping, 'Z' dead). An
        // optional '+' indicates the context is currently running on a CPU.
        process_list::process_state state =
            process_list::process_state::unknown;
        if (stat_tok.size() == 3U) {
            state = process_list::process_state::running;
        } else if (sched_stat == 'R') {
            state = process_list::process_state::running;
        } else if (sched_stat == 'S' || sched_stat == 'B') {
            state = process_list::process_state::sleeping;
        } else if (sched_stat == 'Z') {
            state = process_list::process_state::zombie;
        }

        const std::string_view cpu_tok = read_token();
        if (cpu_tok.empty()) {
            return fail(errc::malformed_data);
        }
        if (cpu_tok != "?") {
            for (char c : cpu_tok) {
                if (!is_digit(c)) {
                    return fail(errc::malformed_data);
                }
            }
        }

        skip_spaces();
        if (pos >= line.size()) {
            return fail(errc::malformed_data);
        }

        // In Redox context.rs, AFFINITY is formatted with {:<11} and TIME with
        // {:<12} as {:02}:{:02}:{:02}.{:02}. If AFFINITY > 11 characters (e.g.
        // long CPU mask), it does not have padding spaces and abuts TIME
        // directly. The first ':' after CPU is always the separator in TIME
        // (HH:MM:SS.ss).
        const std::string_view rem = line.substr(pos);
        const std::size_t colon1 = rem.find(':');
        if (colon1 == std::string_view::npos || colon1 < 2U) {
            return fail(errc::malformed_data);
        }
        if (!is_digit(rem[colon1 - 1U]) || !is_digit(rem[colon1 - 2U])) {
            return fail(errc::malformed_data);
        }
        if (colon1 + 8U >= rem.size()) {
            return fail(errc::malformed_data);
        }
        if (!is_digit(rem[colon1 + 1U]) || !is_digit(rem[colon1 + 2U]) ||
            rem[colon1 + 3U] != ':' || !is_digit(rem[colon1 + 4U]) ||
            !is_digit(rem[colon1 + 5U]) || rem[colon1 + 6U] != '.' ||
            !is_digit(rem[colon1 + 7U]) || !is_digit(rem[colon1 + 8U])) {
            return fail(errc::malformed_data);
        }
        if (colon1 + 9U >= rem.size()) {
            return fail(errc::malformed_data);
        }
        // When CPU time reaches 100+ hours (>= 3 digits), TIME fills all 12
        // bytes and abuts the PRIVATE memory digits directly without spaces.
        const char next_c = rem[colon1 + 9U];
        if (!is_space(next_c) && !is_digit(next_c)) {
            return fail(errc::malformed_data);
        }

        std::string_view affinity = rem.substr(0U, colon1 - 2U);
        while (!affinity.empty() && is_space(affinity.back())) {
            affinity.remove_suffix(1U);
        }
        if (affinity.empty()) {
            return fail(errc::malformed_data);
        }

        pos += colon1 + 9U;

        std::string_view priv_val;
        std::string_view priv_unit;
        if (!parse_memory_field(line, pos, priv_val, priv_unit, true)) {
            return fail(errc::malformed_data);
        }

        std::string_view shared_val;
        std::string_view shared_unit;
        if (!parse_memory_field(line, pos, shared_val, shared_unit, false)) {
            return fail(errc::malformed_data);
        }

        skip_spaces();
        std::string_view raw_name;
        if (pos < line.size()) {
            raw_name = line.substr(pos);
        }
        while (!raw_name.empty() && is_space(raw_name.back())) {
            raw_name.remove_suffix(1U);
        }

        process_list::process_entry entry;
        entry.pid = pid;
        entry.state = state;
        // In Redox, context.name represents individual thread context names,
        // not verified process-level command or executable names. Multiple
        // threads in a process may have different context names. Therefore,
        // process-level name remains nullopt.
        raw_entries.push_back(entry);
    }

    // Redox OS always contains at least the PID 0 [kmain] context. An empty
    // snapshot or header-only content indicates truncated or malformed input.
    if (!saw_header || raw_entries.empty()) {
        return fail(errc::malformed_data);
    }

    // Deterministic O(n log n) deduplication by PID: sort all observed thread
    // contexts by PID and linearly merge adjacent entries, keeping the highest
    // priority process state.
    std::stable_sort(raw_entries.begin(), raw_entries.end(),
                     [](const process_list::process_entry& a,
                        const process_list::process_entry& b) noexcept {
                         return a.pid < b.pid;
                     });

    std::vector<process_list::process_entry> list;
    list.reserve(raw_entries.size());
    for (auto& entry : raw_entries) {
        if (list.empty() || list.back().pid != entry.pid) {
            list.push_back(std::move(entry));
        } else {
            if (process_state_priority(entry.state) >
                process_state_priority(list.back().state)) {
                list.back().state = entry.state;
            }
        }
    }
    return list;
}

struct system_context_reader {
    static result<std::string> read_context_file() {
        int fd = -1;
        for (;;) {
            fd = ::open("/scheme/sys/context", O_RDONLY | O_CLOEXEC);
            if (fd >= 0) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            const int err = errno;
            if (err == ENOENT) {
                return fail(errc::not_supported);
            }
            if (err == EACCES || err == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(err, std::generic_category()));
        }
        struct fd_guard {
            int d;
            ~fd_guard() {
                if (d >= 0) {
                    ::close(d);
                }
            }
        } guard {fd};

        std::string content;
        char buffer[1024];
        for (;;) {
            const ssize_t bytes = ::read(fd, buffer, sizeof(buffer));
            if (bytes == 0) {
                break;
            }
            if (bytes < 0) {
                if (errno == EINTR) {
                    continue;
                }
                const int err = errno;
                if (err == EACCES || err == EPERM) {
                    return fail(errc::permission_denied);
                }
                return fail(std::error_code(err, std::generic_category()));
            }
            if (content.size() + static_cast<std::size_t>(bytes) >
                1024U * 1024U) {
                return fail(errc::value_too_large);
            }
            content.append(buffer, static_cast<std::size_t>(bytes));
        }
        return content;
    }
};

template <typename ContextReader = system_context_reader>
inline result<std::vector<process_list::process_entry>> processes() {
    auto content = ContextReader::read_context_file();
    if (!content) {
        return fail(content.error());
    }
    auto parsed = parse_sys_context(*content);
    if (!parsed) {
        return fail(parsed.error());
    }
    process_list_common::sort_processes(*parsed);
    return parsed;
}

template <typename ContextReader = system_context_reader>
inline result<std::uint32_t> process_count() {
    const auto procs = processes<ContextReader>();
    if (procs) {
        return static_cast<std::uint32_t>(procs->size());
    }
    return fail(procs.error());
}

template <typename ContextReader = system_context_reader>
inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }
    const auto procs = processes<ContextReader>();
    if (!procs) {
        return fail(procs.error());
    }
    for (const auto& entry : *procs) {
        if (entry.pid == pid) {
            return entry;
        }
    }
    return fail(errc::not_found);
}

inline result<std::vector<process_list::process_entry>>
find_processes_by_name(std::string_view name) {
    static_cast<void>(name);
    return fail(errc::not_supported);
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
