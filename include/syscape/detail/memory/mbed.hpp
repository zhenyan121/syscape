#ifndef SYSCAPE_DETAIL_MEMORY_MBED_HPP
#define SYSCAPE_DETAIL_MEMORY_MBED_HPP

#include <syscape/detail/config.hpp>

#include <cstddef>
#include <cstdint>

#if defined(__has_include)
#if __has_include(<mbed.h>)
#include <mbed.h>
#define SYSCAPE_MBED_HAS_KERNEL_HEADERS 1
#elif __has_include(<mbed_stats.h>)
#include <mbed_stats.h>
#define SYSCAPE_MBED_HAS_KERNEL_HEADERS 1
#elif __has_include(<platform/mbed_stats.h>)
#include <platform/mbed_stats.h>
#define SYSCAPE_MBED_HAS_KERNEL_HEADERS 1
#endif
#endif

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> physical_memory_bytes() {
#if defined(SYSCAPE_MBED_HAS_KERNEL_HEADERS)
    mbed_stats_heap_t stats {};
    mbed_stats_heap_get(&stats);
    if (stats.reserved_size > 0) {
        return static_cast<std::uint64_t>(stats.reserved_size);
    }
#endif
#if defined(MBED_TOTAL_HEAP_SIZE) && (MBED_TOTAL_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(MBED_TOTAL_HEAP_SIZE);
#elif defined(MBED_HEAP_SIZE) && (MBED_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(MBED_HEAP_SIZE);
#elif defined(MBED_CONF_TARGET_DEFAULT_HEAP_SIZE) &&                           \
    (MBED_CONF_TARGET_DEFAULT_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(MBED_CONF_TARGET_DEFAULT_HEAP_SIZE);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(MBED_FREE_HEAP_SIZE)
    return static_cast<std::uint64_t>(MBED_FREE_HEAP_SIZE);
#elif defined(SYSCAPE_MBED_HAS_KERNEL_HEADERS)
    mbed_stats_heap_t stats {};
    mbed_stats_heap_get(&stats);
    if (stats.reserved_size > 0) {
        if (stats.reserved_size < stats.current_size) {
            return fail(errc::malformed_data);
        }
        return static_cast<std::uint64_t>(stats.reserved_size -
                                          stats.current_size);
    }
    return fail(errc::not_supported);
#else
    return fail(errc::not_supported);
#endif
}

inline result<memory_common::swap_usage> swap_status() {
    return fail(errc::not_supported);
}

inline result<memory_common::commit_usage> commit_status() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> huge_page_size_bytes() {
    return fail(errc::not_supported);
}

inline result<memory_common::huge_page_pool_usage> huge_page_pool_status() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> memory_load_percent() {
    const auto phys = physical_memory_bytes();
    if (!phys) {
        return fail(phys.error());
    }
    const auto avail = available_memory_bytes();
    if (!avail) {
        return fail(avail.error());
    }
    if (*avail > *phys) {
        return fail(errc::malformed_data);
    }
    return memory_common::utilization_percent(*phys - *avail, *phys);
}

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_MBED_HAS_KERNEL_HEADERS)
#undef SYSCAPE_MBED_HAS_KERNEL_HEADERS
#endif

#endif
