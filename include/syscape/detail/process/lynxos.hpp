#ifndef SYSCAPE_DETAIL_PROCESS_LYNXOS_HPP
#define SYSCAPE_DETAIL_PROCESS_LYNXOS_HPP

/// @file
/// @brief LynxOS / LynxOS-178 RTOS backend implementation for process queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses getpid(), getppid(), getpriority(), and getcwd().

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/resource.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/process/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> process_id() {
#if defined(SYSCAPE_LYNXOS_PID)
    static_assert(SYSCAPE_LYNXOS_PID > 0,
                  "SYSCAPE_LYNXOS_PID must be positive");
    return static_cast<std::uint32_t>(SYSCAPE_LYNXOS_PID);
#elif defined(LYNXOS_PID)
    static_assert(LYNXOS_PID > 0, "LYNXOS_PID must be positive");
    return static_cast<std::uint32_t>(LYNXOS_PID);
#else
    return static_cast<std::uint32_t>(::getpid());
#endif
}

inline result<std::uint32_t> parent_process_id() {
#if defined(SYSCAPE_LYNXOS_PPID)
    static_assert(SYSCAPE_LYNXOS_PPID >= 0,
                  "SYSCAPE_LYNXOS_PPID must not be negative");
    return static_cast<std::uint32_t>(SYSCAPE_LYNXOS_PPID);
#elif defined(LYNXOS_PPID)
    static_assert(LYNXOS_PPID >= 0, "LYNXOS_PPID must not be negative");
    return static_cast<std::uint32_t>(LYNXOS_PPID);
#else
    return static_cast<std::uint32_t>(::getppid());
#endif
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
            std::string path(buf.data());
            if (path.empty()) {
                return fail(errc::malformed_data);
            }
            if (!is_valid_utf8(path)) {
                return fail(errc::invalid_encoding);
            }
            return path;
        }
        const int err = errno;
        if (err == ERANGE) {
            size *= 2U;
            continue;
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err != 0) {
            return fail(std::error_code(err, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    return fail(errc::value_too_large);
}

inline result<process_common::cpu_time_usage> cpu_time() {
    return fail(errc::not_supported);
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
#if defined(SYSCAPE_LYNXOS_PRIORITY)
    return static_cast<int>(SYSCAPE_LYNXOS_PRIORITY);
#elif defined(LYNXOS_PRIORITY)
    return static_cast<int>(LYNXOS_PRIORITY);
#else
    errno = 0;
    const int prio = ::getpriority(PRIO_PROCESS, 0);
    if (errno != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return prio;
#endif
}

inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return fail(errc::not_supported);
}

inline result<process_common::resource_limit_snapshot>
resource_limit(process_common::limit_resource resource) {
    (void)resource;
    return fail(errc::not_supported);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_PROCESS_LYNXOS_HPP
