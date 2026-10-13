#ifndef SYSCAPE_DETAIL_PROCESS_NUCLEUS_HPP
#define SYSCAPE_DETAIL_PROCESS_NUCLEUS_HPP

/// @file
/// @brief Nucleus RTOS backend implementation for process and task queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses <nucleus.h> when available for active task priority.

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#if !defined(SYSCAPE_NUCLEUS_HAS_NUCLEUS_H)
#if defined(__has_include)
#if __has_include(<nucleus.h>)
#include <nucleus.h>
#define SYSCAPE_NUCLEUS_HAS_NUCLEUS_H 1
#elif __has_include("nucleus.h")
#include "nucleus.h"
#define SYSCAPE_NUCLEUS_HAS_NUCLEUS_H 1
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

// Returns the active task's scheduling priority on Nucleus RTOS.
inline result<int> priority() {
#if defined(SYSCAPE_NUCLEUS_HAS_NUCLEUS_H)
    NU_TASK* const task = NU_Current_Task_Pointer();
    if (task == nullptr) {
        return fail(errc::not_found);
    }
    // Provide a 16-byte buffer to safely accommodate NU_MAX_NAME (typically 8
    // bytes) plus potential null termination written by NU_Task_Information.
    char name[16] = {0};
    uint8_t status = 0;
    uint32_t scheduled_count = 0;
    uint8_t prio = 0;
    void* stack_base = nullptr;
    uint32_t stack_size = 0;
    uint32_t min_stack = 0;
    const int status_code =
        NU_Task_Information(task, name, &status, &scheduled_count, &prio,
                            &stack_base, &stack_size, &min_stack);
    if (status_code != 0) {
        return fail(errc::not_found);
    }
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

#endif // SYSCAPE_DETAIL_PROCESS_NUCLEUS_HPP
