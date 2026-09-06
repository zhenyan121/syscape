#ifndef SYSCAPE_DETAIL_PROCESS_LIST_MINIX_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_MINIX_HPP

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

#include <syscape/detail/process_list/common.hpp>
#include <syscape/process_list.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

class dir_handle {
    public:
    explicit dir_handle(::DIR* dir) noexcept : dir_(dir) {}
    dir_handle(const dir_handle&) = delete;
    dir_handle& operator=(const dir_handle&) = delete;
    ~dir_handle() {
        if (dir_ != nullptr) {
            static_cast<void>(::closedir(dir_));
        }
    }

    ::DIR* get() const noexcept {
        return dir_;
    }

    private:
    ::DIR* dir_;
};

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
    DIR* raw_dir = ::opendir("/proc");
    if (raw_dir == nullptr) {
        const int err = errno;
        if (err == ENOENT) {
            return fail(errc::not_supported);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(err, std::generic_category()));
    }
    dir_handle dir(raw_dir);

    std::vector<process_list::process_entry> list;
    struct dirent* ent = nullptr;
    while (true) {
        errno = 0;
        ent = ::readdir(dir.get());
        if (ent == nullptr) {
            const int read_err = errno;
            if (read_err != 0) {
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
        if (::stat(proc_path, &st) != 0) {
            const int stat_error = errno;
            if (stat_error == ENOENT || stat_error == ESRCH) {
                continue;
            }
            if (stat_error == EACCES || stat_error == EPERM) {
                // Inaccessible process is still reported by PID enumeration
            } else if (stat_error != 0) {
                return fail(
                    std::error_code(stat_error, std::generic_category()));
            }
        }
        // MINIX procfs PID directory stat reflects effective UID/GID,
        // while process_entry::uid and gid require real UID/GID.
        // In the absence of a reliable real UID/GID source, leave uid, gid,
        // user_name, and ppid unpopulated (nullopt).

        list.push_back(std::move(entry));
    }

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
    static_cast<void>(name);
    return fail(errc::not_supported);
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
