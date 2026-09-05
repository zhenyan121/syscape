#ifndef SYSCAPE_DETAIL_PROCESS_LIST_SERENITY_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_SERENITY_HPP

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <limits>
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

inline bool parse_pid(const char* s, std::uint32_t& pid) noexcept {
    if (s == nullptr || *s == '\0') {
        return false;
    }
    std::uint32_t value = 0U;
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return false;
        }
        const std::uint32_t digit = static_cast<std::uint32_t>(*s - '0');
        if (value >
            ((std::numeric_limits<std::uint32_t>::max)() - digit) / 10U) {
            return false;
        }
        value = value * 10U + digit;
        ++s;
    }
    if (value == 0U) {
        return false;
    }
    pid = value;
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
        std::uint32_t pid = 0U;
        if (!parse_pid(ent->d_name, pid)) {
            continue;
        }
        process_list::process_entry entry;
        entry.pid = pid;

        char proc_path[64];
        std::snprintf(proc_path, sizeof(proc_path), "/proc/%u",
                      static_cast<unsigned int>(pid));
        struct stat st {};
        errno = 0;
        if (::stat(proc_path, &st) == 0) {
            if (static_cast<std::uintmax_t>(st.st_uid) >
                    (std::numeric_limits<std::uint32_t>::max)() ||
                static_cast<std::uintmax_t>(st.st_gid) >
                    (std::numeric_limits<std::uint32_t>::max)()) {
                ::closedir(dir);
                return fail(errc::value_too_large);
            }
            entry.uid = static_cast<std::uint32_t>(st.st_uid);
            entry.gid = static_cast<std::uint32_t>(st.st_gid);
            auto pwd = posix_passwd::entry_by_uid(st.st_uid);
            if (pwd) {
                entry.user_name = std::move(pwd->name);
            }
        } else {
            const int stat_error = errno;
            if (stat_error == ENOENT || stat_error == ESRCH) {
                continue;
            }
            if (stat_error != EACCES && stat_error != EPERM) {
                ::closedir(dir);
                if (stat_error == 0) {
                    return fail(errc::io_error);
                }
                return fail(
                    std::error_code(stat_error, std::generic_category()));
            }
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

// TODO: Process entries on SerenityOS currently enumerate PIDs and ownership
// but do not extract process names from procfs; searching by name is
// temporarily stubbed as not_supported.
inline result<std::vector<process_list::process_entry>>
find_processes_by_name(std::string_view name) {
    static_cast<void>(name);
    return fail(errc::not_supported);
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
