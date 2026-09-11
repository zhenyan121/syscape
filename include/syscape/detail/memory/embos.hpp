#ifndef SYSCAPE_DETAIL_MEMORY_EMBOS_HPP
#define SYSCAPE_DETAIL_MEMORY_EMBOS_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<RTOS.h>)
#include <RTOS.h>
#define SYSCAPE_EMBOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<rtos.h>)
#include <rtos.h>
#define SYSCAPE_EMBOS_HAS_KERNEL_HEADERS 1
#endif
#endif
#if defined(__cplusplus)
}
#endif

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
    return fail(errc::not_supported);
}

// Returns the total embOS heap capacity configured by OS_TOTAL_HEAP_SIZE or
// OS_HEAP_SIZE. Note: On embedded MCU targets this represents the managed heap
// rather than the total hardware physical RAM/SRAM size.
inline result<std::uint64_t> physical_memory_bytes() {
#if defined(OS_TOTAL_HEAP_SIZE)
    return static_cast<std::uint64_t>(OS_TOTAL_HEAP_SIZE);
#elif defined(OS_HEAP_SIZE)
    return static_cast<std::uint64_t>(OS_HEAP_SIZE);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_EMBOS_HAS_KERNEL_HEADERS)
#if defined(OS_TOTAL_HEAP_SIZE) || defined(OS_HEAP_SIZE)
    return static_cast<std::uint64_t>(::OS_GetFreeHeapSpace());
#else
    return fail(errc::not_supported);
#endif
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

#if defined(SYSCAPE_EMBOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_EMBOS_HAS_KERNEL_HEADERS
#endif

#endif
