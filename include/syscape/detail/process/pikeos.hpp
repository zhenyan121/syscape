#ifndef SYSCAPE_DETAIL_PROCESS_PIKEOS_HPP
#define SYSCAPE_DETAIL_PROCESS_PIKEOS_HPP

/// @file
/// @brief PikeOS RTOS backend implementation for process and task queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses <p4.h> when available for active task priority.

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#if !defined(SYSCAPE_PIKEOS_HAS_P4_H)
#if defined(__has_include)
#if __has_include(<p4.h>)
#include <p4.h>
#define SYSCAPE_PIKEOS_HAS_P4_H 1
#elif __has_include("p4.h")
#include "p4.h"
#define SYSCAPE_PIKEOS_HAS_P4_H 1
#endif
#endif
#endif

#include <syscape/detail/process/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> process_id() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> parent_process_id() {
    return fail(errc::not_supported);
}

inline result<std::string> executable_path() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> command_line() {
    return fail(errc::not_supported);
}

inline result<std::string> working_directory() {
    return fail(errc::not_supported);
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

// Returns the active task's current scheduling priority on PikeOS.
inline result<int> priority() {
#if defined(SYSCAPE_PIKEOS_HAS_P4_H)
    const P4_task_t th = p4_task_self();
    if (th == 0) {
        return fail(errc::not_found);
    }
    const P4_prio_t prio = p4_task_get_prio(th);
    return static_cast<int>(prio);
#else
    return fail(errc::not_supported);
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

#endif // SYSCAPE_DETAIL_PROCESS_PIKEOS_HPP
