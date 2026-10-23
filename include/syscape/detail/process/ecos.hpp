#ifndef SYSCAPE_DETAIL_PROCESS_ECOS_HPP
#define SYSCAPE_DETAIL_PROCESS_ECOS_HPP

/// @file
/// @brief eCos RTOS backend implementation for process and task queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses <cyg/kernel/kapi.h> when available for active thread priority.

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#if !defined(SYSCAPE_ECOS_HAS_KAPI_H)
#if defined(__has_include)
#if __has_include(<cyg/kernel/kapi.h>)
#include <cyg/kernel/kapi.h>
#define SYSCAPE_ECOS_HAS_KAPI_H 1
#elif __has_include("cyg/kernel/kapi.h")
#include "cyg/kernel/kapi.h"
#define SYSCAPE_ECOS_HAS_KAPI_H 1
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

// Returns the active thread's current scheduling priority on eCos.
inline result<int> priority() {
#if defined(SYSCAPE_ECOS_HAS_KAPI_H)
    const cyg_handle_t th = cyg_thread_self();
    if (th == 0) {
        return fail(errc::not_found);
    }
    const cyg_priority_t prio = cyg_thread_get_current_priority(th);
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

#endif // SYSCAPE_DETAIL_PROCESS_ECOS_HPP
