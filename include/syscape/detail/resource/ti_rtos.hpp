#ifndef SYSCAPE_DETAIL_RESOURCE_TI_RTOS_HPP
#define SYSCAPE_DETAIL_RESOURCE_TI_RTOS_HPP

/// @file
/// @brief TI-RTOS (SYS/BIOS) backend implementation for system resource
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses SYSCAPE_TI_RTOS_THREAD_COUNT or TI_RTOS_THREAD_COUNT for thread
/// counts.

#include <syscape/detail/config.hpp>

#include <cstdint>

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
#if defined(SYSCAPE_TI_RTOS_THREAD_COUNT) && (SYSCAPE_TI_RTOS_THREAD_COUNT > 0)
    return static_cast<std::uint64_t>(SYSCAPE_TI_RTOS_THREAD_COUNT);
#elif defined(TI_RTOS_THREAD_COUNT) && (TI_RTOS_THREAD_COUNT > 0)
    return static_cast<std::uint64_t>(TI_RTOS_THREAD_COUNT);
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

#endif // SYSCAPE_DETAIL_RESOURCE_TI_RTOS_HPP
