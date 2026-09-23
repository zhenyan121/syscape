#ifndef SYSCAPE_DETAIL_RESOURCE_RIOT_HPP
#define SYSCAPE_DETAIL_RESOURCE_RIOT_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>

#if !defined(SYSCAPE_RIOT_HAS_KERNEL_HEADERS)
#define SYSCAPE_RIOT_INTERNAL_KERNEL_HEADERS 1
#endif

#if defined(__has_include)
#if __has_include(<kernel_defines.h>)
#include <kernel_defines.h>
#define SYSCAPE_RIOT_HAS_KERNEL_HEADERS 1
#endif
#if __has_include(<thread.h>)
#include <thread.h>
#define SYSCAPE_RIOT_HAS_KERNEL_HEADERS 1
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
#if defined(SYSCAPE_RIOT_HAS_KERNEL_HEADERS)
    const int count = sched_num_threads;
    if (count < 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint64_t>(count);
#elif defined(RIOT_THREAD_COUNT) && (RIOT_THREAD_COUNT > 0)
    return static_cast<std::uint64_t>(RIOT_THREAD_COUNT);
#elif defined(RIOT_MAX_THREADS) && (RIOT_MAX_THREADS > 0)
    return static_cast<std::uint64_t>(RIOT_MAX_THREADS);
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

#if defined(SYSCAPE_RIOT_INTERNAL_KERNEL_HEADERS)
#undef SYSCAPE_RIOT_HAS_KERNEL_HEADERS
#undef SYSCAPE_RIOT_INTERNAL_KERNEL_HEADERS
#endif

#endif
