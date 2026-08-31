#ifndef SYSCAPE_DETAIL_PROCESS_LIST_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_DRAGONFLY_HPP

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <pwd.h>
#include <string>
#include <string_view>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/process_list.hpp>
#include <syscape/detail/process_list/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline process_list::process_state
dragonfly_process_state(char state) noexcept {
    switch (state) {
    case 1: // SIDL
        return process_list::process_state::sleeping;
    case 2: // SACTIVE
        return process_list::process_state::running;
    case 3: // SSTOP
        return process_list::process_state::stopped;
    case 4: // SZOMB
    case 5: // SCORE
        return process_list::process_state::zombie;
    default:
        return process_list::process_state::unknown;
    }
}

inline std::optional<std::string> lookup_username_by_uid(std::uint32_t uid) {
    constexpr std::size_t initial_size = 1024U;
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(initial_size);

    for (;;) {
        ::passwd entry {};
        ::passwd* result_ptr = nullptr;
        const int outcome =
            ::getpwuid_r(static_cast<::uid_t>(uid), &entry, buffer.data(),
                         buffer.size(), &result_ptr);
        if (outcome != 0) {
            if (outcome == ERANGE && buffer.size() < maximum_size) {
                buffer.resize(buffer.size() <= maximum_size / 2U
                                  ? buffer.size() * 2U
                                  : maximum_size);
                continue;
            }
            return std::nullopt;
        }
        if (result_ptr == nullptr || result_ptr->pw_name == nullptr) {
            return std::nullopt;
        }
        std::string uname(result_ptr->pw_name);
        return is_valid_utf8(uname)
                   ? std::optional<std::string>(std::move(uname))
                   : std::nullopt;
    }
}

inline std::optional<std::string> get_process_exe(pid_t pid) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME,
                  static_cast<int>(pid)};
    char path[PATH_MAX] = {};
    std::size_t size = sizeof(path);
    if (::sysctl(mib, 4, path, &size, nullptr, 0U) == 0 && size > 0U) {
        std::string s(path);
        if (is_valid_utf8(s)) {
            return s;
        }
    }
    return std::nullopt;
}

inline std::optional<std::vector<std::string>> get_process_args(pid_t pid) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ARGS, static_cast<int>(pid)};
    std::size_t size = 0U;
    if (::sysctl(mib, 4, nullptr, &size, nullptr, 0U) != 0 || size == 0U) {
        return std::nullopt;
    }
    std::vector<char> buffer(size);
    if (::sysctl(mib, 4, buffer.data(), &size, nullptr, 0U) != 0 ||
        size == 0U) {
        return std::nullopt;
    }

    std::vector<std::string> args;
    std::size_t start = 0U;
    while (start < size) {
        std::size_t end = start;
        while (end < size && buffer[end] != '\0') {
            ++end;
        }
        std::string arg(buffer.data() + start, end - start);
        if (is_valid_utf8(arg)) {
            args.push_back(std::move(arg));
        }
        start = end + 1U;
    }
    return args;
}

inline process_list::process_entry
make_entry_from_kinfo(const struct kinfo_proc& kp, long page_size) {
    process_list::process_entry entry;
    entry.pid = static_cast<std::uint32_t>(kp.kp_pid);
    entry.ppid = static_cast<std::uint32_t>(kp.kp_ppid);
    entry.uid = static_cast<std::uint32_t>(kp.kp_ruid);
    entry.gid = static_cast<std::uint32_t>(kp.kp_rgid);
    entry.priority = static_cast<int>(kp.kp_nice);

    if (entry.uid) {
        entry.user_name = lookup_username_by_uid(*entry.uid);
    }

    std::string comm(kp.kp_comm);
    if (is_valid_utf8(comm)) {
        entry.name = std::move(comm);
    }

    entry.state = dragonfly_process_state(static_cast<char>(kp.kp_stat));

    if (kp.kp_start.tv_sec > 0) {
        const auto secs = std::chrono::seconds(kp.kp_start.tv_sec);
        const auto usecs = std::chrono::microseconds(kp.kp_start.tv_usec);
        entry.start_time = std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                secs + usecs));
    }

    const auto utime = std::chrono::seconds(kp.kp_ru.ru_utime.tv_sec) +
                       std::chrono::microseconds(kp.kp_ru.ru_utime.tv_usec);
    const auto stime = std::chrono::seconds(kp.kp_ru.ru_stime.tv_sec) +
                       std::chrono::microseconds(kp.kp_ru.ru_stime.tv_usec);
    entry.user_cpu_time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(utime);
    entry.kernel_cpu_time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(stime);

    if (page_size > 0 && kp.kp_vm_rssize > 0) {
        entry.resident_memory_bytes =
            static_cast<std::uint64_t>(kp.kp_vm_rssize) *
            static_cast<std::uint64_t>(page_size);
    }
    if (kp.kp_vm_map_size > 0) {
        entry.virtual_memory_bytes =
            static_cast<std::uint64_t>(kp.kp_vm_map_size);
    }
    if (kp.kp_nthreads > 0) {
        entry.thread_count = static_cast<std::uint32_t>(kp.kp_nthreads);
    }

    entry.executable_path = get_process_exe(kp.kp_pid);
    entry.command_line = get_process_args(kp.kp_pid);

    return entry;
}

inline result<std::vector<struct kinfo_proc>> get_all_kinfo_procs() {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    constexpr int max_attempts = 8;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        std::size_t size = 0U;
        if (::sysctl(mib, 4, nullptr, &size, nullptr, 0U) != 0) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size == 0U) {
            return std::vector<struct kinfo_proc>();
        }

        std::vector<struct kinfo_proc> buffer(size / sizeof(struct kinfo_proc) +
                                              16U);
        size = buffer.size() * sizeof(struct kinfo_proc);
        if (::sysctl(mib, 4, buffer.data(), &size, nullptr, 0U) != 0) {
            if (errno == ENOMEM) {
                continue;
            }
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        buffer.resize(size / sizeof(struct kinfo_proc));
        return buffer;
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::vector<process_list::process_entry>> processes() {
    const result<std::vector<struct kinfo_proc>> kprocs = get_all_kinfo_procs();
    if (!kprocs) {
        return fail(kprocs.error());
    }

    const long page_size = ::sysconf(_SC_PAGESIZE);
    std::vector<process_list::process_entry> result_list;
    result_list.reserve(kprocs->size());

    for (const auto& kp : *kprocs) {
        if (kp.kp_pid <= 0) {
            continue;
        }
        result_list.push_back(make_entry_from_kinfo(kp, page_size));
    }

    std::sort(result_list.begin(), result_list.end(),
              [](const process_list::process_entry& a,
                 const process_list::process_entry& b) noexcept {
                  return a.pid < b.pid;
              });

    return result_list;
}

inline result<std::uint32_t> process_count() {
    const result<std::vector<struct kinfo_proc>> kprocs = get_all_kinfo_procs();
    if (!kprocs) {
        return fail(kprocs.error());
    }
    std::uint32_t count = 0U;
    for (const auto& kp : *kprocs) {
        if (kp.kp_pid > 0) {
            ++count;
        }
    }
    return count;
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }
    if (pid > static_cast<std::uint32_t>((std::numeric_limits<pid_t>::max)())) {
        return fail(errc::value_too_large);
    }
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
    struct kinfo_proc kp {};
    std::size_t size = sizeof(kp);
    if (::sysctl(mib, 4, &kp, &size, nullptr, 0U) != 0) {
        if (errno == ESRCH || errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U || kp.kp_pid <= 0) {
        return fail(errc::not_found);
    }
    const long page_size = ::sysconf(_SC_PAGESIZE);
    return make_entry_from_kinfo(kp, page_size);
}

inline result<std::vector<process_list::process_entry>>
find_processes_by_name(std::string_view name) {
    if (name.empty()) {
        return std::vector<process_list::process_entry> {};
    }
    const result<std::vector<process_list::process_entry>> all = processes();
    if (!all) {
        return fail(all.error());
    }
    std::vector<process_list::process_entry> matches;
    for (const auto& proc : *all) {
        if (process_list_common::matches_process_name(proc, name, false)) {
            matches.push_back(proc);
        }
    }
    return matches;
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
