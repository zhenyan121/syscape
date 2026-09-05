#ifndef SYSCAPE_DETAIL_PROCESS_LIST_HURD_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_HURD_HPP

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/posix/passwd.hpp>
#include <syscape/detail/process_list/common.hpp>
#include <syscape/process_list.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline bool is_numeric(const char* s) noexcept {
    if (s == nullptr || *s == '\0') {
        return false;
    }
    while (*s != '\0') {
        if (!std::isdigit(static_cast<unsigned char>(*s))) {
            return false;
        }
        ++s;
    }
    return true;
}

inline result<std::vector<process_list::process_entry>> processes() {
    DIR* dir = ::opendir("/proc");
    if (dir == nullptr) {
        const int err = errno;
        if (err == ENOENT) {
            return fail(errc::not_supported);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(err, std::generic_category()));
    }

    std::vector<process_list::process_entry> list;
    struct dirent* ent = nullptr;
    while (true) {
        errno = 0;
        ent = ::readdir(dir);
        if (ent == nullptr) {
            const int read_err = errno;
            if (read_err != 0) {
                ::closedir(dir);
                if (read_err == EACCES || read_err == EPERM) {
                    return fail(errc::permission_denied);
                }
                return fail(std::error_code(read_err, std::generic_category()));
            }
            break;
        }
        if (!is_numeric(ent->d_name)) {
            continue;
        }
        const unsigned long raw_pid = std::strtoul(ent->d_name, nullptr, 10);
        if (raw_pid == 0 ||
            raw_pid > (std::numeric_limits<std::uint32_t>::max)()) {
            continue;
        }
        process_list::process_entry entry;
        entry.pid = static_cast<std::uint32_t>(raw_pid);

        char proc_path[64];
        std::snprintf(proc_path, sizeof(proc_path), "/proc/%lu", raw_pid);
        struct stat st {};
        if (::stat(proc_path, &st) == 0) {
            entry.uid = static_cast<std::uint32_t>(st.st_uid);
            entry.gid = static_cast<std::uint32_t>(st.st_gid);
            auto pwd = posix_passwd::entry_by_uid(st.st_uid);
            if (pwd) {
                entry.user_name = std::move(pwd->name);
            }
        }

        char stat_path[64];
        std::snprintf(stat_path, sizeof(stat_path), "/proc/%lu/stat", raw_pid);
        FILE* sfp = std::fopen(stat_path, "r");
        if (sfp != nullptr) {
            char buf[512];
            if (std::fgets(buf, static_cast<int>(sizeof(buf)), sfp) !=
                nullptr) {
                char* open_paren = std::strchr(buf, '(');
                char* close_paren = std::strrchr(buf, ')');
                if (open_paren != nullptr && close_paren != nullptr &&
                    close_paren > open_paren) {
                    entry.name = std::string(open_paren + 1, close_paren);
                    char state_ch = ' ';
                    unsigned long ppid_val = 0;
                    if (std::sscanf(close_paren + 1, " %c %lu", &state_ch,
                                    &ppid_val) >= 2) {
                        if (ppid_val > 0) {
                            entry.ppid = static_cast<std::uint32_t>(ppid_val);
                        }
                        switch (state_ch) {
                        case 'R':
                            entry.state = process_list::process_state::running;
                            break;
                        case 'S':
                        case 'D':
                            entry.state = process_list::process_state::sleeping;
                            break;
                        case 'T':
                            entry.state = process_list::process_state::stopped;
                            break;
                        case 'Z':
                            entry.state = process_list::process_state::zombie;
                            break;
                        default:
                            entry.state = process_list::process_state::unknown;
                            break;
                        }
                    }
                }
            }
            std::fclose(sfp);
        }

        list.push_back(std::move(entry));
    }
    ::closedir(dir);

    process_list_common::sort_processes(list);
    return list;
}

inline result<std::uint32_t> process_count() {
    const auto procs = processes();
    if (procs) {
        return static_cast<std::uint32_t>(procs->size());
    }
    return fail(procs.error());
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }
    const auto procs = processes();
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
    if (name.empty()) {
        return std::vector<process_list::process_entry> {};
    }
    const auto procs = processes();
    if (!procs) {
        return fail(procs.error());
    }
    std::vector<process_list::process_entry> matches;
    for (const auto& entry : *procs) {
        if (process_list_common::matches_process_name(entry, name, false)) {
            matches.push_back(entry);
        }
    }
    return matches;
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
