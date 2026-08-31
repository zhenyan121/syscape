#ifndef SYSCAPE_DETAIL_PROCESS_LIST_NETBSD_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_NETBSD_HPP

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <pwd.h>
#include <string>
#include <string_view>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/process_list/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/process_list.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline result<std::chrono::nanoseconds>
timeval_to_nanoseconds(std::int64_t seconds, std::int64_t microseconds) {
    if (seconds < 0 || microseconds < 0 || microseconds >= 1000000) {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t nanoseconds_per_second = 1000000000U;
    constexpr std::uint64_t nanoseconds_per_microsecond = 1000U;
    const std::uint64_t whole = static_cast<std::uint64_t>(seconds);
    const std::uint64_t fraction =
        static_cast<std::uint64_t>(microseconds) * nanoseconds_per_microsecond;
    const std::uint64_t maximum =
        static_cast<std::uint64_t>((std::chrono::nanoseconds::max)().count());
    if (whole > maximum / nanoseconds_per_second ||
        whole * nanoseconds_per_second > maximum - fraction) {
        return fail(errc::value_too_large);
    }
    return std::chrono::nanoseconds(whole * nanoseconds_per_second + fraction);
}

inline result<std::chrono::system_clock::time_point>
timeval_to_time_point(std::int64_t seconds, std::int64_t microseconds) {
    if (seconds < 0 || microseconds < 0 || microseconds >= 1000000) {
        return fail(errc::malformed_data);
    }
    using clock = std::chrono::system_clock;
    const std::int64_t maximum_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(clock::duration::max())
            .count();
    if (seconds > maximum_seconds) {
        return fail(errc::value_too_large);
    }
    const clock::duration whole = std::chrono::duration_cast<clock::duration>(
        std::chrono::seconds(seconds));
    const clock::duration fraction =
        std::chrono::duration_cast<clock::duration>(
            std::chrono::microseconds(microseconds));
    if (fraction > clock::duration::max() - whole) {
        return fail(errc::value_too_large);
    }
    return clock::time_point(whole + fraction);
}

inline process_list::process_state netbsd_process_state(char state) noexcept {
    switch (state) {
    case 2: // SACTIVE / SRUN
    case 7: // SONPROC
        return process_list::process_state::running;
    case 1: // SIDL
    case 3: // SSLEEP
        return process_list::process_state::sleeping;
    case 4: // SSTOP
        return process_list::process_state::stopped;
    case 5: // SZOMB
    case 6: // SDEAD
        return process_list::process_state::zombie;
    default:
        return process_list::process_state::unknown;
    }
}

inline result<std::optional<std::string>>
lookup_username_by_uid(std::uint32_t uid) {
    constexpr std::size_t initial_size = 1024U;
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(initial_size);

    for (;;) {
        ::passwd entry {};
        ::passwd* result_ptr = nullptr;
        const int outcome =
            ::getpwuid_r(static_cast<::uid_t>(uid), &entry, buffer.data(),
                         buffer.size(), &result_ptr);
        if (outcome == 0 && result_ptr != nullptr && entry.pw_name != nullptr) {
            std::string name(entry.pw_name);
            if (!is_valid_utf8(name)) {
                return fail(errc::invalid_encoding);
            }
            return std::optional<std::string>(std::move(name));
        }
        if (outcome == 0) {
            return std::optional<std::string>();
        }
        if (outcome == ERANGE) {
            if (buffer.size() >= maximum_size) {
                return fail(errc::value_too_large);
            }
            buffer.resize(buffer.size() * 2U);
            continue;
        }
        return std::optional<std::string>();
    }
}

inline result<std::optional<std::string>>
get_process_executable_path(pid_t pid) {
#if defined(KERN_PROC_PATHNAME)
    int mib[] = {CTL_KERN, KERN_PROC_ARGS, pid, KERN_PROC_PATHNAME};
    constexpr int maximum_attempts = 4;
    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        std::size_t size = 0U;
        if (::sysctl(mib, 4U, nullptr, &size, nullptr, 0U) != 0) {
            if (errno == ENOENT || errno == ESRCH || errno == EINVAL) {
                return std::optional<std::string>();
            }
            return std::optional<std::string>();
        }
        if (size == 0U) {
            return std::optional<std::string>();
        }
        std::string path(size, '\0');
        if (::sysctl(mib, 4U, &path[0], &size, nullptr, 0U) != 0) {
            if (errno == ENOMEM) {
                continue;
            }
            if (errno == ENOENT || errno == ESRCH || errno == EINVAL) {
                return std::optional<std::string>();
            }
            return std::optional<std::string>();
        }
        if (size > path.size()) {
            continue;
        }
        path.resize(size);
        while (!path.empty() && path.back() == '\0') {
            path.pop_back();
        }
        if (!is_valid_utf8(path)) {
            return fail(errc::invalid_encoding);
        }
        return path.empty() ? std::optional<std::string>()
                            : std::optional<std::string>(std::move(path));
    }
    return fail(errc::temporarily_unavailable);
#else
    static_cast<void>(pid);
    return std::optional<std::string>();
#endif
}

inline result<std::optional<std::vector<std::string>>>
get_process_command_line(pid_t pid) {
    int mib[] = {CTL_KERN, KERN_PROC_ARGS, pid, KERN_PROC_ARGV};
    constexpr int maximum_attempts = 4;
    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        std::size_t size = 0U;
        if (::sysctl(mib, 4U, nullptr, &size, nullptr, 0U) != 0) {
            if (errno == ENOENT || errno == ESRCH || errno == EINVAL) {
                return std::optional<std::vector<std::string>>();
            }
            return std::optional<std::vector<std::string>>();
        }
        if (size == 0U) {
            return std::optional<std::vector<std::string>>();
        }

        std::vector<char> buffer(size);
        if (::sysctl(mib, 4U, buffer.data(), &size, nullptr, 0U) != 0) {
            if (errno == ENOMEM) {
                continue;
            }
            if (errno == ENOENT || errno == ESRCH || errno == EINVAL) {
                return std::optional<std::vector<std::string>>();
            }
            return std::optional<std::vector<std::string>>();
        }
        if (size > buffer.size()) {
            continue;
        }

        std::vector<std::string> args;
        std::size_t start = 0U;
        for (std::size_t i = 0; i < size; ++i) {
            if (buffer[i] == '\0') {
                if (i > start) {
                    std::string arg(buffer.data() + start, i - start);
                    if (!is_valid_utf8(arg)) {
                        return fail(errc::invalid_encoding);
                    }
                    args.push_back(std::move(arg));
                }
                start = i + 1U;
            }
        }
        if (start != size) {
            return fail(errc::malformed_data);
        }
        return std::optional<std::vector<std::string>>(std::move(args));
    }
    return fail(errc::temporarily_unavailable);
}

inline result<::syscape::process_list::process_entry>
make_entry_from_kinfo2(const struct kinfo_proc2& kp, std::uint64_t page_size) {
    ::syscape::process_list::process_entry entry;
    entry.pid = static_cast<std::uint32_t>(kp.p_pid);
    if (kp.p_ppid < 0) {
        return fail(errc::malformed_data);
    }
    entry.ppid = static_cast<std::uint32_t>(kp.p_ppid);
    entry.uid = static_cast<std::uint32_t>(kp.p_ruid);
    entry.gid = static_cast<std::uint32_t>(kp.p_rgid);
    const result<std::optional<std::string>> user_name =
        lookup_username_by_uid(*entry.uid);
    if (!user_name) {
        return fail(user_name.error());
    }
    entry.user_name = *user_name;

    std::string name(kp.p_comm, ::strnlen(kp.p_comm, sizeof(kp.p_comm)));
    if (!is_valid_utf8(name)) {
        return fail(errc::invalid_encoding);
    }
    if (!name.empty()) {
        entry.name = std::move(name);
    }

    entry.state = netbsd_process_state(kp.p_stat);

    if (kp.p_vm_rssize > 0 && page_size > 0) {
        constexpr std::uint64_t max_u64 =
            (std::numeric_limits<std::uint64_t>::max)();
        const std::uint64_t rss_pages =
            static_cast<std::uint64_t>(kp.p_vm_rssize);
        if (rss_pages > max_u64 / page_size) {
            return fail(errc::value_too_large);
        }
        entry.resident_memory_bytes = rss_pages * page_size;
    }
    if (kp.p_vm_vsize > 0 && page_size > 0) {
        constexpr std::uint64_t max_u64 =
            (std::numeric_limits<std::uint64_t>::max)();
        const std::uint64_t vs_pages =
            static_cast<std::uint64_t>(kp.p_vm_vsize);
        if (vs_pages > max_u64 / page_size) {
            return fail(errc::value_too_large);
        }
        entry.virtual_memory_bytes = vs_pages * page_size;
    }

    if (kp.p_uvalid != 0) {
        // p_ustart_usec, p_uutime_usec, p_ustime_usec are uint32_t and may
        // hold values >= 1000000 on certain kernel threads even when p_uvalid
        // is set.  Treat these timing fields as best-effort: skip them rather
        // than failing the entire process-list query.
        const auto st =
            timeval_to_time_point(static_cast<std::int64_t>(kp.p_ustart_sec),
                                  static_cast<std::int64_t>(kp.p_ustart_usec));
        if (st) {
            entry.start_time = *st;
        } else if (st.error() != errc::malformed_data &&
                   st.error() != errc::value_too_large) {
            return fail(st.error());
        }

        const auto ut =
            timeval_to_nanoseconds(static_cast<std::int64_t>(kp.p_uutime_sec),
                                   static_cast<std::int64_t>(kp.p_uutime_usec));
        if (ut) {
            entry.user_cpu_time = *ut;
        } else if (ut.error() != errc::malformed_data &&
                   ut.error() != errc::value_too_large) {
            return fail(ut.error());
        }

        const auto kt =
            timeval_to_nanoseconds(static_cast<std::int64_t>(kp.p_ustime_sec),
                                   static_cast<std::int64_t>(kp.p_ustime_usec));
        if (kt) {
            entry.kernel_cpu_time = *kt;
        } else if (kt.error() != errc::malformed_data &&
                   kt.error() != errc::value_too_large) {
            return fail(kt.error());
        }
    }

    const result<std::optional<std::string>> executable_path =
        get_process_executable_path(kp.p_pid);
    if (!executable_path) {
        return fail(executable_path.error());
    }
    entry.executable_path = *executable_path;

    const result<std::optional<std::vector<std::string>>> command_line =
        get_process_command_line(kp.p_pid);
    if (!command_line) {
        return fail(command_line.error());
    }
    entry.command_line = *command_line;

    if (kp.p_nlwps > 0) {
        if (static_cast<std::uint64_t>(kp.p_nlwps) >
            (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        entry.thread_count = static_cast<std::uint32_t>(kp.p_nlwps);
    } else {
        entry.thread_count = std::nullopt;
    }

#ifndef NZERO
    constexpr int nzero = 20;
#else
    constexpr int nzero = NZERO;
#endif
    entry.priority = static_cast<int>(kp.p_nice) - nzero;

    return entry;
}

inline result<std::vector<struct kinfo_proc2>> get_all_kinfo_proc2() {
    constexpr int maximum_attempts = 4;
    int mib[] = {CTL_KERN,
                 KERN_PROC2,
                 KERN_PROC_ALL,
                 0,
                 static_cast<int>(sizeof(struct kinfo_proc2)),
                 (std::numeric_limits<int>::max)()};

    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        std::size_t size = 0U;
        if (::sysctl(mib, 6U, nullptr, &size, nullptr, 0U) != 0) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size == 0U) {
            return std::vector<struct kinfo_proc2>();
        }

        if (size % sizeof(struct kinfo_proc2) != 0U) {
            return fail(errc::malformed_data);
        }
        std::size_t count = size / sizeof(struct kinfo_proc2);
        constexpr std::size_t padding = 16U;
        constexpr std::size_t maximum_count =
            (std::numeric_limits<std::size_t>::max)() /
            sizeof(struct kinfo_proc2);
        if (count > maximum_count || count / 4U > maximum_count - count ||
            padding > maximum_count - count - count / 4U) {
            return fail(errc::value_too_large);
        }
        count += count / 4U + padding;
        if (count >
            static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
            return fail(errc::value_too_large);
        }
        std::vector<struct kinfo_proc2> procs(count);
        size = count * sizeof(struct kinfo_proc2);

        mib[5] = static_cast<int>(count);
        if (::sysctl(mib, 6U, procs.data(), &size, nullptr, 0U) != 0) {
            if (errno == ENOMEM) {
                continue;
            }
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size % sizeof(struct kinfo_proc2) != 0) {
            return fail(errc::malformed_data);
        }

        const std::size_t actual_count = size / sizeof(struct kinfo_proc2);
        procs.resize(actual_count);
        return procs;
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::vector<::syscape::process_list::process_entry>> processes() {
    const result<std::vector<struct kinfo_proc2>> procs = get_all_kinfo_proc2();
    if (!procs) {
        return fail(procs.error());
    }

    errno = 0;
    const long page_size_val = ::sysconf(_SC_PAGESIZE);
    if (page_size_val <= 0) {
        return errno != 0
                   ? result<
                         std::vector<::syscape::process_list::process_entry>>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<
                         std::vector<::syscape::process_list::process_entry>>(
                         fail(errc::malformed_data));
    }
    const std::uint64_t page_size = static_cast<std::uint64_t>(page_size_val);

    std::vector<::syscape::process_list::process_entry> results;
    results.reserve(procs->size());

    for (const auto& kp : *procs) {
        if (kp.p_pid <= 0) {
            continue;
        }
        auto entry_res = make_entry_from_kinfo2(kp, page_size);
        if (!entry_res) {
            return fail(entry_res.error());
        }
        results.push_back(std::move(*entry_res));
    }

    std::sort(results.begin(), results.end(),
              [](const ::syscape::process_list::process_entry& a,
                 const ::syscape::process_list::process_entry& b) noexcept {
                  return a.pid < b.pid;
              });

    return results;
}

inline result<std::uint32_t> process_count() {
    const result<std::vector<struct kinfo_proc2>> procs = get_all_kinfo_proc2();
    if (!procs) {
        return fail(procs.error());
    }
    std::uint32_t count = 0U;
    for (const auto& kp : *procs) {
        if (kp.p_pid > 0) {
            if (count == (std::numeric_limits<std::uint32_t>::max)()) {
                return fail(errc::value_too_large);
            }
            ++count;
        }
    }
    return count;
}

inline result<::syscape::process_list::process_entry>
find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }
    if (pid > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
        return fail(errc::value_too_large);
    }
    int mib[] = {CTL_KERN,
                 KERN_PROC2,
                 KERN_PROC_PID,
                 static_cast<int>(pid),
                 static_cast<int>(sizeof(struct kinfo_proc2)),
                 1};
    struct kinfo_proc2 kp {};
    std::size_t size = sizeof(kp);
    if (::sysctl(mib, 6U, &kp, &size, nullptr, 0U) != 0) {
        if (errno == ESRCH || errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(kp) || kp.p_pid <= 0) {
        return fail(errc::malformed_data);
    }

    errno = 0;
    const long page_size_val = ::sysconf(_SC_PAGESIZE);
    if (page_size_val <= 0) {
        return errno != 0
                   ? result<::syscape::process_list::process_entry>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<::syscape::process_list::process_entry>(
                         fail(errc::malformed_data));
    }
    const std::uint64_t page_size = static_cast<std::uint64_t>(page_size_val);
    return make_entry_from_kinfo2(kp, page_size);
}

inline result<std::vector<::syscape::process_list::process_entry>>
find_processes_by_name(std::string_view name) {
    if (name.empty()) {
        return std::vector<::syscape::process_list::process_entry> {};
    }
    const result<std::vector<::syscape::process_list::process_entry>> all =
        processes();
    if (!all) {
        return fail(all.error());
    }
    std::vector<::syscape::process_list::process_entry> matches;
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
