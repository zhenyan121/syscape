#ifndef SYSCAPE_DETAIL_PROCESS_AIX_HPP
#define SYSCAPE_DETAIL_PROCESS_AIX_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <sys/resource.h>
#include <sys/times.h>

#if defined(__has_include)
#if __has_include(<procinfo.h>)
#include <procinfo.h>
#define SYSCAPE_HAS_AIX_PROCINFO 1
#endif
#endif

#include <syscape/detail/process/common.hpp>
#include <syscape/detail/process/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> process_id() {
    return static_cast<std::uint32_t>(::getpid());
}

inline result<std::uint32_t> parent_process_id() {
    return static_cast<std::uint32_t>(::getppid());
}

inline result<std::string> executable_path() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> command_line() {
    return fail(errc::not_supported);
}

inline result<std::string> working_directory() {
    std::size_t size = 1024U;
    constexpr std::size_t max_size = 1024U * 1024U;
    while (size <= max_size) {
        std::string buf(size, '\0');
        errno = 0;
        if (::getcwd(&buf[0], size) != nullptr) {
            buf.resize(std::strlen(buf.c_str()));
            return buf;
        }
        const int err = errno;
        if (err == ERANGE) {
            size *= 2U;
            continue;
        }
        if (err == ENOENT) {
            return fail(errc::not_found);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(err, std::generic_category()));
    }
    return fail(errc::value_too_large);
}

inline result<std::chrono::nanoseconds>
clock_ticks_to_nanoseconds(std::uint64_t ticks, long ticks_per_second) {
    constexpr std::uint64_t nanoseconds_per_second = 1000000000U;
    if (ticks_per_second <= 0 ||
        static_cast<std::uint64_t>(ticks_per_second) >
            (std::numeric_limits<std::uint64_t>::max)() /
                nanoseconds_per_second) {
        return fail(errc::not_supported);
    }
    const std::uint64_t rate = static_cast<std::uint64_t>(ticks_per_second);
    const std::uint64_t whole_seconds = ticks / rate;
    const std::uint64_t maximum_nanoseconds =
        static_cast<std::uint64_t>((std::chrono::nanoseconds::max)().count());
    if (whole_seconds > maximum_nanoseconds / nanoseconds_per_second) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t remainder_nanoseconds =
        (ticks % rate) * nanoseconds_per_second / rate;
    const std::uint64_t whole_nanoseconds =
        whole_seconds * nanoseconds_per_second;
    if (whole_nanoseconds > maximum_nanoseconds - remainder_nanoseconds) {
        return fail(errc::value_too_large);
    }
    using nanoseconds_rep = std::chrono::nanoseconds::rep;
    return std::chrono::nanoseconds(static_cast<nanoseconds_rep>(
        whole_nanoseconds + remainder_nanoseconds));
}

inline result<process_common::cpu_time_usage> cpu_time() {
    struct ::tms t {};
    if (::times(&t) != static_cast<clock_t>(-1)) {
        const long clk = ::sysconf(_SC_CLK_TCK);
        if (clk > 0) {
            if (t.tms_utime < static_cast<clock_t>(0) ||
                t.tms_stime < static_cast<clock_t>(0)) {
                return fail(errc::malformed_data);
            }
            const auto user = clock_ticks_to_nanoseconds(
                static_cast<std::uint64_t>(t.tms_utime), clk);
            if (!user) {
                return fail(user.error());
            }
            const auto system = clock_ticks_to_nanoseconds(
                static_cast<std::uint64_t>(t.tms_stime), clk);
            if (!system) {
                return fail(system.error());
            }
            process_common::cpu_time_usage usage;
            usage.user = *user;
            usage.system = *system;
            return usage;
        }
    }
    return fail(errc::not_supported);
}

inline result<std::chrono::system_clock::time_point> start_time() {
#if defined(SYSCAPE_HAS_AIX_PROCINFO)
    struct procentry64 pe {};
    pid_t pid = ::getpid();
    if (::getprocs64(&pe, sizeof(pe), nullptr, 0, &pid, 1) > 0 &&
        pe.pi_pid == ::getpid() && pe.pi_start > 0) {
        return std::chrono::system_clock::time_point(
            std::chrono::seconds(pe.pi_start));
    }
#endif
    return fail(errc::not_supported);
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
#if defined(SYSCAPE_HAS_AIX_PROCINFO)
    struct procentry64 pe {};
    pid_t pid = ::getpid();
    if (::getprocs64(&pe, sizeof(pe), nullptr, 0, &pid, 1) > 0 &&
        pe.pi_pid == ::getpid()) {
        process_common::memory_usage_snapshot mem {};
        mem.resident_bytes =
            static_cast<std::uint64_t>(pe.pi_drss + pe.pi_trss) * 4096ULL;
        mem.virtual_bytes = static_cast<std::uint64_t>(pe.pi_size) * 4096ULL;
        return mem;
    }
#endif
    struct ::rusage ru {};
    if (::getrusage(RUSAGE_SELF, &ru) == 0) {
        process_common::memory_usage_snapshot mem {};
        mem.resident_bytes = static_cast<std::uint64_t>(ru.ru_maxrss) * 1024ULL;
        return mem;
    }
    return fail(errc::not_supported);
}

inline result<std::uint32_t> thread_count() {
#if defined(SYSCAPE_HAS_AIX_PROCINFO)
    struct procentry64 pe {};
    pid_t pid = ::getpid();
    if (::getprocs64(&pe, sizeof(pe), nullptr, 0, &pid, 1) > 0 &&
        pe.pi_pid == ::getpid() && pe.pi_thcount > 0) {
        return static_cast<std::uint32_t>(pe.pi_thcount);
    }
#endif
    return static_cast<std::uint32_t>(1U);
}

inline result<int> priority() {
    return process_posix::priority(-20, 19);
}

inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return fail(errc::not_supported);
}

inline result<process_common::resource_limit_snapshot>
resource_limit(process_common::limit_resource resource) {
    return process_posix::resource_limit(resource);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
