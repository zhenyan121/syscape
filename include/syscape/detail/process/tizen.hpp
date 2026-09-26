#ifndef SYSCAPE_DETAIL_PROCESS_TIZEN_HPP
#define SYSCAPE_DETAIL_PROCESS_TIZEN_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include <syscape/detail/process/common.hpp>
#include <syscape/detail/process/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

template <typename T>
inline result<std::uint32_t> validate_positive_u32_count(T value) {
    if constexpr (std::is_signed<T>::value) {
        if (value <= 0) {
            return fail(errc::malformed_data);
        }
    } else {
        if (value == 0) {
            return fail(errc::malformed_data);
        }
    }
    if (static_cast<unsigned long long>(value) >
        static_cast<unsigned long long>(
            (std::numeric_limits<std::uint32_t>::max)())) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(value);
}

template <typename T>
inline result<std::uint32_t> validate_u32_id(T value) {
    if constexpr (std::is_signed<T>::value) {
        if (value < 0) {
            return fail(errc::malformed_data);
        }
    }
    if (static_cast<unsigned long long>(value) >
        static_cast<unsigned long long>(
            (std::numeric_limits<std::uint32_t>::max)())) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(value);
}

/// Returns the calling process identifier if supplied by override macro.
inline result<std::uint32_t> process_id() {
#if defined(SYSCAPE_TIZEN_PID)
    return validate_positive_u32_count(SYSCAPE_TIZEN_PID);
#else
    return fail(errc::not_supported);
#endif
}

/// Returns the parent process identifier if supplied by override macro.
inline result<std::uint32_t> parent_process_id() {
#if defined(SYSCAPE_TIZEN_PPID)
    return validate_u32_id(SYSCAPE_TIZEN_PPID);
#else
    return fail(errc::not_supported);
#endif
}

/// Executable path query is not supported in unprivileged sandboxes.
inline result<std::string> executable_path() {
    return fail(errc::not_supported);
}

/// Command line arguments are not exposed across unprivileged sandboxes.
inline result<std::vector<std::string>> command_line() {
    return fail(errc::not_supported);
}

/// Working directory query is not supported without filesystem access.
inline result<std::string> working_directory() {
    return fail(errc::not_supported);
}

/// CPU time accounting is not available in unprivileged application sandboxes.
inline result<process_common::cpu_time_usage> cpu_time() {
    return fail(errc::not_supported);
}

/// Process start time is not exposed in unprivileged application sandboxes.
inline result<std::chrono::system_clock::time_point> start_time() {
    return fail(errc::not_supported);
}

/// Process memory usage metrics are not exposed without procfs access.
inline result<process_common::memory_usage_snapshot> memory_usage() {
    return fail(errc::not_supported);
}

/// Returns the process thread count if supplied by override macro.
inline result<std::uint32_t> thread_count() {
#if defined(SYSCAPE_TIZEN_THREAD_COUNT)
    return validate_positive_u32_count(SYSCAPE_TIZEN_THREAD_COUNT);
#else
    return fail(errc::not_supported);
#endif
}

/// Returns the process priority if supplied by override macro.
inline result<int> priority() {
#if defined(SYSCAPE_TIZEN_TASK_PRIORITY)
    return process_posix::validate_priority(SYSCAPE_TIZEN_TASK_PRIORITY, -20,
                                            19);
#elif defined(SYSCAPE_TIZEN_PRIORITY)
    return process_posix::validate_priority(SYSCAPE_TIZEN_PRIORITY, -20, 19);
#else
    return fail(errc::not_supported);
#endif
}

/// CPU affinity is not exposed in unprivileged sandboxes.
inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return fail(errc::not_supported);
}

/// Resource limit queries are not supported without getrlimit permissions.
inline result<process_common::resource_limit_snapshot>
resource_limit(process_common::limit_resource /*resource*/) {
    return fail(errc::not_supported);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
