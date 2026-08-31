#ifndef SYSCAPE_DETAIL_PROCESS_NETBSD_HPP
#define SYSCAPE_DETAIL_PROCESS_NETBSD_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/process/common.hpp>
#include <syscape/detail/process/posix.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

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

inline result<std::uint32_t> process_id() {
    const pid_t pid = ::getpid();
    if (pid <= 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(pid);
}

inline result<std::uint32_t> parent_process_id() {
    const pid_t ppid = ::getppid();
    if (ppid < 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(ppid);
}

inline result<std::string> executable_path() {
#if defined(KERN_PROC_PATHNAME)
    int mib[] = {CTL_KERN, KERN_PROC_ARGS, ::getpid(), KERN_PROC_PATHNAME};
    std::size_t size = 0U;
    if (::sysctl(mib, 4U, nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return fail(errc::not_found);
    }

    std::string path(size, '\0');
    if (::sysctl(mib, 4U, &path[0], &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size > path.size()) {
        return fail(errc::temporarily_unavailable);
    }
    path.resize(size);
    while (!path.empty() && path.back() == '\0') {
        path.pop_back();
    }
    if (path.empty()) {
        return fail(errc::not_found);
    }
    if (!is_valid_utf8(path)) {
        return fail(errc::invalid_encoding);
    }
    return path;
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::vector<std::string>> command_line() {
    int mib[] = {CTL_KERN, KERN_PROC_ARGS, ::getpid(), KERN_PROC_ARGV};
    std::size_t size = 0U;
    if (::sysctl(mib, 4U, nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return std::vector<std::string>();
    }

    std::vector<char> buffer(size);
    if (::sysctl(mib, 4U, buffer.data(), &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size > buffer.size()) {
        return fail(errc::temporarily_unavailable);
    }

    std::vector<std::string> args;
    std::size_t start = 0U;
    for (std::size_t i = 0; i < size; ++i) {
        if (buffer[i] == '\0') {
            if (i > start) {
                std::string argument(buffer.data() + start, i - start);
                if (!is_valid_utf8(argument)) {
                    return fail(errc::invalid_encoding);
                }
                args.push_back(std::move(argument));
            }
            start = i + 1U;
        }
    }
    if (start != size) {
        return fail(errc::malformed_data);
    }
    return args;
}

inline result<std::string> working_directory() {
#if defined(KERN_PROC_CWD)
    int mib[] = {CTL_KERN, KERN_PROC_ARGS, ::getpid(), KERN_PROC_CWD};
    std::size_t size = 0U;
    if (::sysctl(mib, 4U, nullptr, &size, nullptr, 0U) == 0 && size > 0U) {
        std::string path(size, '\0');
        if (::sysctl(mib, 4U, &path[0], &size, nullptr, 0U) == 0) {
            if (size > path.size()) {
                return fail(errc::temporarily_unavailable);
            }
            path.resize(size);
            while (!path.empty() && path.back() == '\0') {
                path.pop_back();
            }
            if (!path.empty() && is_valid_utf8(path)) {
                return path;
            }
            if (!path.empty()) {
                return fail(errc::invalid_encoding);
            }
        }
    }
#endif
    char buffer[PATH_MAX];
    if (::getcwd(buffer, sizeof(buffer)) != nullptr) {
        std::string path(buffer);
        if (!is_valid_utf8(path)) {
            return fail(errc::invalid_encoding);
        }
        return path;
    }
    return fail(std::error_code(errno, std::generic_category()));
}

inline result<struct kinfo_proc2> query_kinfo_proc2(pid_t pid) {
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
    if (size != sizeof(kp)) {
        return fail(errc::malformed_data);
    }
    return kp;
}

inline result<process_common::cpu_time_usage> cpu_time() {
    struct ::rusage usage {};
    if (::getrusage(RUSAGE_SELF, &usage) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const result<std::chrono::nanoseconds> user = timeval_to_nanoseconds(
        static_cast<std::int64_t>(usage.ru_utime.tv_sec),
        static_cast<std::int64_t>(usage.ru_utime.tv_usec));
    if (!user) {
        return fail(user.error());
    }
    const result<std::chrono::nanoseconds> system = timeval_to_nanoseconds(
        static_cast<std::int64_t>(usage.ru_stime.tv_sec),
        static_cast<std::int64_t>(usage.ru_stime.tv_usec));
    if (!system) {
        return fail(system.error());
    }
    process_common::cpu_time_usage times;
    times.user = *user;
    times.system = *system;
    return times;
}

inline result<std::chrono::system_clock::time_point> start_time() {
    const auto kp = query_kinfo_proc2(::getpid());
    if (!kp) {
        return fail(kp.error());
    }
    if (kp->p_uvalid == 0) {
        return fail(errc::temporarily_unavailable);
    }
    return timeval_to_time_point(static_cast<std::int64_t>(kp->p_ustart_sec),
                                 static_cast<std::int64_t>(kp->p_ustart_usec));
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
    const auto kp = query_kinfo_proc2(::getpid());
    if (!kp) {
        return fail(kp.error());
    }
    errno = 0;
    const long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        return errno != 0
                   ? result<process_common::memory_usage_snapshot>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<process_common::memory_usage_snapshot>(
                         fail(errc::malformed_data));
    }
    if (kp->p_vm_rssize < 0 || kp->p_vm_vsize < 0) {
        return fail(errc::malformed_data);
    }
    const std::uint64_t resident_pages =
        static_cast<std::uint64_t>(kp->p_vm_rssize);
    const std::uint64_t virtual_pages =
        static_cast<std::uint64_t>(kp->p_vm_vsize);
    const std::uint64_t page_bytes = static_cast<std::uint64_t>(page_size);
    constexpr std::uint64_t max_u64 =
        (std::numeric_limits<std::uint64_t>::max)();
    if (resident_pages > max_u64 / page_bytes ||
        virtual_pages > max_u64 / page_bytes) {
        return fail(errc::value_too_large);
    }
    process_common::memory_usage_snapshot mem;
    mem.resident_bytes = resident_pages * page_bytes;
    mem.virtual_bytes = virtual_pages * page_bytes;
    return mem;
}

inline result<std::uint32_t> thread_count() {
    const auto kp = query_kinfo_proc2(::getpid());
    if (!kp) {
        return fail(kp.error());
    }
    if (kp->p_nlwps <= 0) {
        return fail(errc::malformed_data);
    }
    if (static_cast<std::uint64_t>(kp->p_nlwps) >
        (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(kp->p_nlwps);
}

inline result<int> priority() {
    return process_posix::priority(-20, 20);
}

inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return fail(errc::not_supported);
}

inline result<process_common::resource_limit_snapshot>
resource_limit(process_common::limit_resource kind) {
    return process_posix::resource_limit(kind);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
