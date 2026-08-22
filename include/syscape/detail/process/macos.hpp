#ifndef SYSCAPE_DETAIL_PROCESS_MACOS_HPP
#define SYSCAPE_DETAIL_PROCESS_MACOS_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <array>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/sysctl.h>

#include <syscape/detail/process/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> process_id_value(pid_t value,
                                              bool zero_is_valid) {
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
    std::vector<char> buffer(1024U);
    std::uint32_t size = static_cast<std::uint32_t>(buffer.size());
    int status = ::_NSGetExecutablePath(buffer.data(), &size);
    if (status == -1) {
        if (size == 0U) { return fail(errc::malformed_data); }
        buffer.resize(size);
        size = static_cast<std::uint32_t>(buffer.size());
        status = ::_NSGetExecutablePath(buffer.data(), &size);
    }
    if (status != 0) { return fail(errc::io_error); }

    std::size_t length = 0U;
    while (length < buffer.size() && buffer[length] != '\0') { ++length; }
    if (length >= buffer.size()) { return fail(errc::malformed_data); }
    const std::string path(buffer.data(), length);
    return path.empty() || path.front() != '/'
        ? result<std::string>(fail(errc::malformed_data))
        : result<std::string>(path);
}

inline result<std::vector<std::string>> command_line() {
    const int* count_pointer = ::_NSGetArgc();
    char*** argument_storage = ::_NSGetArgv();
    if (count_pointer == nullptr || argument_storage == nullptr ||
        *argument_storage == nullptr) {
        return fail(errc::not_found);
    }

    const int count = *count_pointer;
    if (count < 0) { return fail(errc::malformed_data); }
    std::vector<std::string> values;
    if (static_cast<std::size_t>(count) > values.max_size()) {
        return fail(errc::resource_exhausted);
    }
    values.resize(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        const char* argument = (*argument_storage)[index];
        if (argument == nullptr) { return fail(errc::malformed_data); }
        values[static_cast<std::size_t>(index)] = std::string(argument);
    }
    return values;
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
        buffer.resize(buffer.size() <= maximum_size / 2U
                          ? buffer.size() * 2U
                          : maximum_size);
    }
}

/// Converts a nanosecond amount reported by the platform to a duration.
inline result<std::chrono::nanoseconds> nanoseconds_amount(
    std::uint64_t count) {
    constexpr std::uint64_t maximum = static_cast<std::uint64_t>(
        (std::chrono::nanoseconds::max)().count());
    if (count > maximum) { return fail(errc::value_too_large); }
    return std::chrono::nanoseconds(count);
}

/// Builds a system-clock time point from a wall-clock seconds and
/// microseconds pair as recorded by the platform.
inline result<std::chrono::system_clock::time_point>
timeval_components_to_time_point(std::int64_t seconds,
                                 std::int64_t microseconds) {
    using clock = std::chrono::system_clock;
    if (seconds < 0 || microseconds < 0 || microseconds >= 1000000) {
        return fail(errc::malformed_data);
    }
    const auto maximum_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            clock::duration::max()).count();
    if (seconds > maximum_seconds) { return fail(errc::value_too_large); }
    const clock::duration whole =
        std::chrono::duration_cast<clock::duration>(
            std::chrono::seconds(seconds));
    const clock::duration fraction =
        std::chrono::duration_cast<clock::duration>(
            std::chrono::microseconds(microseconds));
    if (fraction > clock::duration::max() - whole) {
        return fail(errc::value_too_large);
    }
    return clock::time_point(whole + fraction);
}

/// Parsed runtime-attribute fields of one PROC_PIDTASKINFO snapshot.
struct task_statistics {
    /// Total user-mode execution time in nanoseconds.
    std::uint64_t user_nanoseconds = 0U;
    /// Total kernel-mode execution time in nanoseconds.
    std::uint64_t system_nanoseconds = 0U;
    /// Physical memory occupied by the task in bytes.
    std::uint64_t resident_bytes = 0U;
    /// Virtual address-space extent of the task in bytes.
    std::uint64_t virtual_bytes = 0U;
    /// Number of live threads. Always at least one.
    std::uint32_t threads = 0U;
};

/// Validates and converts the signed thread count reported by proc_taskinfo.
inline result<std::uint32_t> task_thread_count(std::int32_t count) {
    if (count <= 0) { return fail(errc::malformed_data); }
    return static_cast<std::uint32_t>(count);
}

/// Reads the documented proc_taskinfo fields for the calling process.
inline result<task_statistics> read_task_statistics() {
    struct ::proc_taskinfo info {};
    errno = 0;
    const int copied = ::proc_pidinfo(
        ::getpid(), ::PROC_PIDTASKINFO, 0, &info,
        static_cast<int>(sizeof(info)));
    if (copied <= 0) {
        return fail(errno != 0
                        ? std::error_code(errno, std::generic_category())
                        : make_error_code(errc::io_error));
    }
    if (static_cast<std::size_t>(copied) < sizeof(info)) {
        return fail(errc::malformed_data);
    }

    task_statistics statistics;
    statistics.user_nanoseconds = info.pti_total_user;
    statistics.system_nanoseconds = info.pti_total_system;
    statistics.resident_bytes = info.pti_resident_size;
    statistics.virtual_bytes = info.pti_virtual_size;
    const result<std::uint32_t> threads =
        task_thread_count(info.pti_threadnum);
    if (!threads) { return fail(threads.error()); }
    statistics.threads = *threads;
    return statistics;
}

inline result<process_common::cpu_time_usage> cpu_time() {
    const result<task_statistics> statistics = read_task_statistics();
    if (!statistics) { return fail(statistics.error()); }
    const result<std::chrono::nanoseconds> user =
        nanoseconds_amount(statistics->user_nanoseconds);
    if (!user) { return fail(user.error()); }
    const result<std::chrono::nanoseconds> system =
        nanoseconds_amount(statistics->system_nanoseconds);
    if (!system) { return fail(system.error()); }
    process_common::cpu_time_usage usage;
    usage.user = *user;
    usage.system = *system;
    return usage;
}

inline result<std::chrono::system_clock::time_point> start_time() {
    const pid_t self = ::getpid();
    if (self <= 0) { return fail(errc::malformed_data); }
    std::array<int, 4> request {CTL_KERN, KERN_PROC, KERN_PROC_PID,
                                static_cast<int>(self)};
    struct ::kinfo_proc information {};
    std::size_t size = sizeof(information);
    if (::sysctl(request.data(), static_cast<u_int>(request.size()),
                 &information, &size, nullptr, 0) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(information)) { return fail(errc::malformed_data); }
    return timeval_components_to_time_point(
        static_cast<std::int64_t>(information.kp_proc.p_starttime.tv_sec),
        static_cast<std::int64_t>(information.kp_proc.p_starttime.tv_usec));
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
    const result<task_statistics> statistics = read_task_statistics();
    if (!statistics) { return fail(statistics.error()); }
    process_common::memory_usage_snapshot usage;
    usage.resident_bytes = statistics->resident_bytes;
    usage.virtual_bytes = statistics->virtual_bytes;
    return usage;
}

inline result<std::uint32_t> thread_count() {
    const result<task_statistics> statistics = read_task_statistics();
    if (!statistics) { return fail(statistics.error()); }
    return statistics->threads;
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
