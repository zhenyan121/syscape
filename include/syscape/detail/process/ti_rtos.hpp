#ifndef SYSCAPE_DETAIL_PROCESS_TI_RTOS_HPP
#define SYSCAPE_DETAIL_PROCESS_TI_RTOS_HPP

/// @file
/// @brief TI-RTOS (SYS/BIOS) backend implementation for process and task
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses <ti/sysbios/knl/Task.h> when available for active task priority.

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#if defined(__has_include)
#if __has_include(<ti/sysbios/knl/Task.h>)
#include <ti/sysbios/knl/Task.h>
#define SYSCAPE_TI_RTOS_HAS_TASK_H 1
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

// Returns the active task's scheduling priority on TI-RTOS (SYS/BIOS).
inline result<int> priority() {
#if defined(SYSCAPE_TI_RTOS_HAS_TASK_H)
    const Task_Handle task = Task_self();
    if (task == nullptr) {
        return fail(errc::not_found);
    }
    const int prio = Task_getPri(task);
    if (prio < 0) {
        return fail(errc::not_found);
    }
    return prio;
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

#if defined(SYSCAPE_TI_RTOS_HAS_TASK_H)
#undef SYSCAPE_TI_RTOS_HAS_TASK_H
#endif

#endif // SYSCAPE_DETAIL_PROCESS_TI_RTOS_HPP
