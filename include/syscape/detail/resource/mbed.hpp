#ifndef SYSCAPE_DETAIL_RESOURCE_MBED_HPP
#define SYSCAPE_DETAIL_RESOURCE_MBED_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>

#if defined(__has_include)
#if __has_include(<mbed.h>)
#include <mbed.h>
#define SYSCAPE_MBED_HAS_KERNEL_HEADERS 1
#elif __has_include(<cmsis_os2.h>)
#include <cmsis_os2.h>
#define SYSCAPE_MBED_HAS_KERNEL_HEADERS 1
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
#if defined(SYSCAPE_MBED_HAS_KERNEL_HEADERS)
    return static_cast<std::uint64_t>(osThreadGetCount());
#elif defined(MBED_THREAD_COUNT) && (MBED_THREAD_COUNT > 0)
    return static_cast<std::uint64_t>(MBED_THREAD_COUNT);
#elif defined(MBED_TASK_COUNT) && (MBED_TASK_COUNT > 0)
    return static_cast<std::uint64_t>(MBED_TASK_COUNT);
#elif defined(MBED_MAX_THREADS) && (MBED_MAX_THREADS > 0)
    return static_cast<std::uint64_t>(MBED_MAX_THREADS);
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

#if defined(SYSCAPE_MBED_HAS_KERNEL_HEADERS)
#undef SYSCAPE_MBED_HAS_KERNEL_HEADERS
#endif

#endif
