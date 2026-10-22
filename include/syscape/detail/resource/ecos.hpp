#ifndef SYSCAPE_DETAIL_RESOURCE_ECOS_HPP
#define SYSCAPE_DETAIL_RESOURCE_ECOS_HPP

/// @file
/// @brief eCos RTOS backend implementation for system resource queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses SYSCAPE_ECOS_THREAD_COUNT or ECOS_THREAD_COUNT for thread counts.

#include <syscape/detail/config.hpp>

#include <cstdint>

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
#if defined(SYSCAPE_ECOS_THREAD_COUNT) && (SYSCAPE_ECOS_THREAD_COUNT > 0)
    return static_cast<std::uint64_t>(SYSCAPE_ECOS_THREAD_COUNT);
#elif defined(ECOS_THREAD_COUNT) && (ECOS_THREAD_COUNT > 0)
    return static_cast<std::uint64_t>(ECOS_THREAD_COUNT);
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

#endif // SYSCAPE_DETAIL_RESOURCE_ECOS_HPP
