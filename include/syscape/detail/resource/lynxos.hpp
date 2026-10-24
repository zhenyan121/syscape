#ifndef SYSCAPE_DETAIL_RESOURCE_LYNXOS_HPP
#define SYSCAPE_DETAIL_RESOURCE_LYNXOS_HPP

/// @file
/// @brief LynxOS / LynxOS-178 RTOS backend implementation for system resource
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses sysconf(_SC_OPEN_MAX) and SYSCAPE_LYNXOS_THREAD_COUNT /
/// TASK_COUNT macros.

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <unistd.h>

#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

inline result<resource_common::load_samples> load_average() {
    return fail(errc::not_supported);
}

inline result<resource_common::entity_counts> scheduler_entities() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> process_count() {
#if defined(SYSCAPE_LYNXOS_PROCESS_COUNT)
    static_assert(SYSCAPE_LYNXOS_PROCESS_COUNT > 0,
                  "SYSCAPE_LYNXOS_PROCESS_COUNT must be positive");
    return static_cast<std::uint64_t>(SYSCAPE_LYNXOS_PROCESS_COUNT);
#elif defined(LYNXOS_PROCESS_COUNT)
    static_assert(LYNXOS_PROCESS_COUNT > 0,
                  "LYNXOS_PROCESS_COUNT must be positive");
    return static_cast<std::uint64_t>(LYNXOS_PROCESS_COUNT);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> thread_count() {
#if defined(SYSCAPE_LYNXOS_THREAD_COUNT)
    static_assert(SYSCAPE_LYNXOS_THREAD_COUNT > 0,
                  "SYSCAPE_LYNXOS_THREAD_COUNT must be positive");
    return static_cast<std::uint64_t>(SYSCAPE_LYNXOS_THREAD_COUNT);
#elif defined(LYNXOS_THREAD_COUNT)
    static_assert(LYNXOS_THREAD_COUNT > 0,
                  "LYNXOS_THREAD_COUNT must be positive");
    return static_cast<std::uint64_t>(LYNXOS_THREAD_COUNT);
#elif defined(SYSCAPE_LYNXOS_TASK_COUNT)
    static_assert(SYSCAPE_LYNXOS_TASK_COUNT > 0,
                  "SYSCAPE_LYNXOS_TASK_COUNT must be positive");
    return static_cast<std::uint64_t>(SYSCAPE_LYNXOS_TASK_COUNT);
#elif defined(LYNXOS_TASK_COUNT)
    static_assert(LYNXOS_TASK_COUNT > 0, "LYNXOS_TASK_COUNT must be positive");
    return static_cast<std::uint64_t>(LYNXOS_TASK_COUNT);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> open_file_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> open_handle_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> file_descriptor_limit() {
#if defined(_SC_OPEN_MAX)
    const long limit = ::sysconf(_SC_OPEN_MAX);
    if (limit > 0) {
        return static_cast<std::uint64_t>(limit);
    }
#endif
    return fail(errc::not_supported);
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_RESOURCE_LYNXOS_HPP
