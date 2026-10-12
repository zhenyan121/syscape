#ifndef SYSCAPE_DETAIL_RESOURCE_NUCLEUS_HPP
#define SYSCAPE_DETAIL_RESOURCE_NUCLEUS_HPP

/// @file
/// @brief Nucleus RTOS backend implementation for system resource queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses <nucleus.h> (NU_Established_Tasks) or
/// SYSCAPE_NUCLEUS_THREAD_COUNT for thread counts.

#include <syscape/detail/config.hpp>

#include <cstdint>

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
    return fail(errc::not_supported);
}

inline result<std::uint64_t> thread_count() {
#if defined(SYSCAPE_NUCLEUS_THREAD_COUNT) && (SYSCAPE_NUCLEUS_THREAD_COUNT > 0)
    return static_cast<std::uint64_t>(SYSCAPE_NUCLEUS_THREAD_COUNT);
#elif defined(NUCLEUS_THREAD_COUNT) && (NUCLEUS_THREAD_COUNT > 0)
    return static_cast<std::uint64_t>(NUCLEUS_THREAD_COUNT);
#elif defined(SYSCAPE_NUCLEUS_HAS_NUCLEUS_H)
    const std::uint32_t tasks = NU_Established_Tasks();
    if (tasks > 0U) {
        return static_cast<std::uint64_t>(tasks);
    }
    return fail(errc::not_supported);
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
    return fail(errc::not_supported);
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_RESOURCE_NUCLEUS_HPP
