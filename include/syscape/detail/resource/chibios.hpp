#ifndef SYSCAPE_DETAIL_RESOURCE_CHIBIOS_HPP
#define SYSCAPE_DETAIL_RESOURCE_CHIBIOS_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<ch.h>)
#include <ch.h>
#define SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS 1
#endif
#endif
#if defined(__cplusplus)
}
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
#if defined(SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS) &&                             \
    ((defined(CH_CFG_USE_REGISTRY) && (CH_CFG_USE_REGISTRY != 0)) ||           \
     (defined(CH_USE_REGISTRY) && (CH_USE_REGISTRY != 0)))
    std::uint64_t count = 0;
    for (thread_t* tp = chRegFirstThread(); tp != nullptr;
         tp = chRegNextThread(tp)) {
        ++count;
    }
    return count;
#elif defined(CH_CFG_MAX_THREADS) && (CH_CFG_MAX_THREADS > 0)
    return static_cast<std::uint64_t>(CH_CFG_MAX_THREADS);
#elif defined(CH_TASK_COUNT) && (CH_TASK_COUNT > 0)
    return static_cast<std::uint64_t>(CH_TASK_COUNT);
#elif defined(CH_CFG_NUM_THREADS) && (CH_CFG_NUM_THREADS > 0)
    return static_cast<std::uint64_t>(CH_CFG_NUM_THREADS);
#elif defined(CH_NUM_THREADS) && (CH_NUM_THREADS > 0)
    return static_cast<std::uint64_t>(CH_NUM_THREADS);
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

#if defined(SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS
#endif

#endif
