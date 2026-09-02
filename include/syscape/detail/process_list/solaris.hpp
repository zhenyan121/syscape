#ifndef SYSCAPE_DETAIL_PROCESS_LIST_SOLARIS_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_SOLARIS_HPP

#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <optional>
#include <pwd.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#if defined(__has_include)
#if __has_include(<procfs.h>)
#include <procfs.h>
#define SYSCAPE_HAS_PROCFS 1
#endif
#endif

#include <syscape/detail/posix/passwd.hpp>
#include <syscape/detail/process_list/common.hpp>
#include <syscape/process_list.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline bool is_all_digits(const char* str) {
    if (str == nullptr || str[0] == '\0') {
        return false;
    }
    for (const char* p = str; *p != '\0'; ++p) {
        if (!std::isdigit(static_cast<unsigned char>(*p))) {
            return false;
        }
    }
    return true;
}

#if defined(SYSCAPE_HAS_PROCFS)

inline result<std::string> read_symlink_exact(const std::string& path) {
    std::size_t size = 512U;
    for (int attempts = 0; attempts < 4; ++attempts) {
        std::string buffer(size, '\0');
        const ssize_t len = ::readlink(path.c_str(), &buffer[0], size);
        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (static_cast<std::size_t>(len) < size) {
            buffer.resize(static_cast<std::size_t>(len));
            return buffer;
        }
        size *= 2U;
    }
    return fail(errc::malformed_data);
}

inline result<process_list::process_entry> read_proc_entry(std::uint32_t pid) {
    const std::string pid_str = std::to_string(pid);
    const std::string psinfo_path = "/proc/" + pid_str + "/psinfo";

    const int fd = ::open(psinfo_path.c_str(), O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT || errno == ESRCH) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    ::psinfo_t info {};
    char* target = reinterpret_cast<char*>(&info);
    std::size_t total_read = 0U;
    const std::size_t required = sizeof(info);
    while (total_read < required) {
        const ssize_t bytes =
            ::read(fd, target + total_read, required - total_read);
        if (bytes > 0) {
            total_read += static_cast<std::size_t>(bytes);
        } else if (bytes == 0) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else {
            const int saved_errno = errno;
            ::close(fd);
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
    }
    ::close(fd);
    if (total_read != required) {
        return fail(errc::malformed_data);
    }

    process_list::process_entry entry;
    entry.pid = static_cast<std::uint32_t>(info.pr_pid);
    entry.ppid = static_cast<std::uint32_t>(info.pr_ppid);
    entry.uid = static_cast<std::uint32_t>(info.pr_uid);
    entry.gid = static_cast<std::uint32_t>(info.pr_gid);

    if (info.pr_fname[0] != '\0') {
        entry.name = std::string(info.pr_fname);
    }

    // Resolve username
    const auto pwd_entry = posix_passwd::entry_by_uid(info.pr_uid);
    if (pwd_entry && !pwd_entry->name.empty()) {
        entry.user_name = pwd_entry->name;
    }

    // Paths
    const auto exe = read_symlink_exact("/proc/" + pid_str + "/path/a.out");
    if (exe && !exe->empty()) {
        entry.executable_path = *exe;
    }

    const auto cwd = read_symlink_exact("/proc/" + pid_str + "/path/cwd");
    if (cwd && !cwd->empty()) {
        entry.working_directory = *cwd;
    }

    // Memory
    const auto rss_kib = static_cast<std::uint64_t>(info.pr_rssize);
    const auto virt_kib = static_cast<std::uint64_t>(info.pr_size);
    if (rss_kib <= UINT64_MAX / 1024ULL) {
        entry.resident_memory_bytes = rss_kib * 1024ULL;
    }
    if (virt_kib <= UINT64_MAX / 1024ULL) {
        entry.virtual_memory_bytes = virt_kib * 1024ULL;
    }

    // Threads
    if (info.pr_nlwp > 0) {
        entry.thread_count = static_cast<std::uint32_t>(info.pr_nlwp);
    }

    // Priority
    entry.priority = static_cast<int>(info.pr_pri);

    // State
    if (info.pr_zomb != 0 || info.pr_lwp.pr_sname == 'Z') {
        entry.state = process_list::process_state::zombie;
    } else {
        switch (info.pr_lwp.pr_sname) {
        case 'O':
        case 'R':
            entry.state = process_list::process_state::running;
            break;
        case 'S':
        case 'W':
        case 'I':
            entry.state = process_list::process_state::sleeping;
            break;
        case 'T':
            entry.state = process_list::process_state::stopped;
            break;
        default:
            entry.state = process_list::process_state::unknown;
            break;
        }
    }

    // Start time
    if (info.pr_start.tv_sec > 0) {
        const auto sec = std::chrono::seconds(info.pr_start.tv_sec);
        const auto nsec = std::chrono::nanoseconds(info.pr_start.tv_nsec);
        entry.start_time = std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                sec + nsec));
    }

    return entry;
}

#endif // SYSCAPE_HAS_PROCFS

inline result<std::vector<process_list::process_entry>> processes() {
#if defined(SYSCAPE_HAS_PROCFS)
    DIR* dir = ::opendir("/proc");
    if (dir == nullptr) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    struct dir_guard {
        DIR* d;
        ~dir_guard() {
            if (d != nullptr) {
                ::closedir(d);
            }
        }
    } guard {dir};

    std::vector<process_list::process_entry> list;
    for (;;) {
        errno = 0;
        struct ::dirent* ent = ::readdir(dir);
        if (ent == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (is_all_digits(ent->d_name)) {
            const unsigned long pid_val =
                std::strtoul(ent->d_name, nullptr, 10);
            if (pid_val > 0) {
                const auto pid = static_cast<std::uint32_t>(pid_val);
                auto entry = read_proc_entry(pid);
                if (entry) {
                    list.push_back(std::move(*entry));
                } else if (entry.error() == errc::permission_denied ||
                           entry.error() == std::errc::permission_denied) {
                    process_list::process_entry restricted {};
                    restricted.pid = pid;
                    restricted.state = process_list::process_state::unknown;
                    list.push_back(std::move(restricted));
                } else if (entry.error() != errc::not_found) {
                    return fail(entry.error());
                }
            }
        }
    }
    process_list_common::sort_processes(list);
    return list;
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint32_t> process_count() {
    const auto list = processes();
    if (!list) {
        return fail(list.error());
    }
    if (list->size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(list->size());
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
#if defined(SYSCAPE_HAS_PROCFS)
    return read_proc_entry(pid);
#else
    static_cast<void>(pid);
    return fail(errc::not_supported);
#endif
}

inline result<std::vector<process_list::process_entry>>
find_processes_by_name(std::string_view name) {
    const auto all = processes();
    if (!all) {
        return fail(all.error());
    }
    std::vector<process_list::process_entry> matches;
    for (const auto& p : *all) {
        if (process_list_common::matches_process_name(p, name, false)) {
            matches.push_back(p);
        }
    }
    return matches;
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
