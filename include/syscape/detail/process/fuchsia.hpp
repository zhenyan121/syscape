#ifndef SYSCAPE_DETAIL_PROCESS_FUCHSIA_HPP
#define SYSCAPE_DETAIL_PROCESS_FUCHSIA_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <limits>
#include <string>
#include <sys/resource.h>
#include <sys/times.h>
#include <system_error>
#include <unistd.h>
#include <vector>

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
    std::size_t size = 256U;
    constexpr std::size_t max_size = 1024U * 1024U;
    while (size <= max_size) {
        std::vector<char> buf(size);
        errno = 0;
        if (::getcwd(buf.data(), size) != nullptr) {
            return std::string(buf.data());
        }
        const int err = errno;
        if (err == ERANGE) {
            if (size > max_size / 2U) {
                return fail(errc::value_too_large);
            }
            size *= 2U;
            continue;
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err == ENOSYS || err == ENOTSUP) {
            return fail(errc::not_supported);
        }
        if (err != 0) {
            return fail(std::error_code(err, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    return fail(errc::value_too_large);
}

inline result<std::chrono::nanoseconds>
clock_ticks_to_nanoseconds(std::uint64_t ticks, long ticks_per_second) {
    if (ticks_per_second <= 0) {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t nanoseconds_per_second = 1000000000ULL;
    if (static_cast<std::uint64_t>(ticks_per_second) >
        (std::numeric_limits<std::uint64_t>::max)() / nanoseconds_per_second) {
        return fail(errc::value_too_large);
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
    errno = 0;
    if (::times(&t) == static_cast<clock_t>(-1)) {
        const int saved_errno = errno;
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    errno = 0;
    const long clk = ::sysconf(_SC_CLK_TCK);
    if (clk <= 0) {
        const int saved_errno = errno;
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
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

inline result<std::chrono::system_clock::time_point> start_time() {
    return fail(errc::not_supported);
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> thread_count() {
    return fail(errc::not_supported);
}

inline result<int> priority() {
#if defined(PRIO_MIN) && defined(PRIO_MAX)
    return process_posix::priority(PRIO_MIN, PRIO_MAX);
#else
    return process_posix::priority(-20, 20);
#endif
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
