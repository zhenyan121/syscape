#ifndef SYSCAPE_DETAIL_PROCESS_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_PROCESS_DRAGONFLY_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/resource.h>

#include <syscape/detail/process/common.hpp>
#include <syscape/detail/process/posix.hpp>
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

inline result<std::uint32_t> process_id_value(pid_t value, bool zero_is_valid) {
    if (value < 0 || (!zero_is_valid && value == 0)) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(value);
}

inline result<std::uint32_t> process_id() {
    return process_id_value(::getpid(), false);
}

inline result<std::uint32_t> parent_process_id() {
    return process_id_value(::getppid(), true);
}

inline result<std::string> executable_path() {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(1024U);
    for (;;) {
        std::size_t size = buffer.size();
        if (::sysctl(mib, 4, buffer.data(), &size, nullptr, 0U) == 0) {
            if (size > buffer.size()) {
                return fail(errc::malformed_data);
            }
            std::size_t length = 0U;
            while (length < size && buffer[length] != '\0') {
                ++length;
            }
            const std::string path(buffer.data(), length);
            return path.empty() || path.front() != '/'
                       ? result<std::string>(fail(errc::malformed_data))
                       : result<std::string>(path);
        }
        if (errno != ENOMEM) {
            std::vector<char> link_buf(1024U);
            const ssize_t len = ::readlink(
                "/proc/curproc/file", link_buf.data(), link_buf.size() - 1U);
            if (len > 0) {
                link_buf[static_cast<std::size_t>(len)] = '\0';
                return std::string(link_buf.data(),
                                   static_cast<std::size_t>(len));
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (buffer.size() >= maximum_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() * 2U);
    }
}

inline result<std::vector<std::string>> command_line() {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ARGS, ::getpid()};
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(1024U);
    for (;;) {
        std::size_t size = buffer.size();
        if (::sysctl(mib, 4, buffer.data(), &size, nullptr, 0U) == 0) {
            if (size == 0U) {
                return std::vector<std::string> {};
            }
            if (size > buffer.size()) {
                return fail(errc::malformed_data);
            }
            std::vector<std::string> arguments;
            std::size_t start = 0U;
            for (std::size_t index = 0U; index < size; ++index) {
                if (buffer[index] == '\0') {
                    arguments.emplace_back(buffer.data() + start,
                                           index - start);
                    start = index + 1U;
                }
            }
            if (start < size) {
                arguments.emplace_back(buffer.data() + start, size - start);
            }
            return arguments;
        }
        if (errno != ENOMEM) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (buffer.size() >= maximum_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() * 2U);
    }
}

inline result<std::string> working_directory() {
    std::vector<char> buffer(1024U);
    constexpr std::size_t maximum_size = 1024U * 1024U;
    for (;;) {
        errno = 0;
        const char* value = ::getcwd(buffer.data(), buffer.size());
        if (value != nullptr) {
            const std::string path(buffer.data());
            return path.empty() || path.front() != '/'
                       ? result<std::string>(fail(errc::malformed_data))
                       : result<std::string>(std::move(path));
        }
        if (errno != ERANGE) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (buffer.size() >= maximum_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() <= maximum_size / 2U ? buffer.size() * 2U
                                                         : maximum_size);
    }
}

inline result<struct ::kinfo_proc> query_kinfo_proc(pid_t pid) {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
    struct ::kinfo_proc proc {};
    std::size_t size = sizeof(proc);
    if (::sysctl(mib, 4, &proc, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return fail(errc::not_found);
    }
    if (size != sizeof(proc)) {
        return fail(errc::malformed_data);
    }
    return proc;
}

inline result<std::chrono::system_clock::time_point> start_time() {
    const result<struct ::kinfo_proc> proc = query_kinfo_proc(::getpid());
    if (!proc) {
        return fail(proc.error());
    }
    return timeval_to_time_point(
        static_cast<std::int64_t>(proc->kp_start.tv_sec),
        static_cast<std::int64_t>(proc->kp_start.tv_usec));
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

inline result<process_common::memory_usage_snapshot> memory_usage() {
    const result<struct ::kinfo_proc> proc = query_kinfo_proc(::getpid());
    if (!proc) {
        return fail(proc.error());
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
    const std::uint64_t resident_pages =
        static_cast<std::uint64_t>(proc->kp_vm_rssize);
    const std::uint64_t page_bytes = static_cast<std::uint64_t>(page_size);
    constexpr std::uint64_t max_u64 =
        (std::numeric_limits<std::uint64_t>::max)();
    if (resident_pages > max_u64 / page_bytes) {
        return fail(errc::value_too_large);
    }
    process_common::memory_usage_snapshot snapshot;
    snapshot.resident_bytes = resident_pages * page_bytes;
    snapshot.virtual_bytes = static_cast<std::uint64_t>(proc->kp_vm_map_size);
    return snapshot;
}

inline result<std::uint32_t> thread_count() {
    const result<struct ::kinfo_proc> proc = query_kinfo_proc(::getpid());
    if (!proc) {
        return fail(proc.error());
    }
    if (proc->kp_nthreads <= 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(proc->kp_nthreads);
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
