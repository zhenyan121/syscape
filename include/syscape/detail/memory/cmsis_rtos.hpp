#ifndef SYSCAPE_DETAIL_MEMORY_CMSIS_RTOS_HPP
#define SYSCAPE_DETAIL_MEMORY_CMSIS_RTOS_HPP

#include <syscape/detail/config.hpp>

#include <cstddef>
#include <cstdint>

#if defined(__has_include)
#if defined(__CMSIS_RTOS) && !defined(__CMSIS_RTOS2) && __has_include(<cmsis_os.h>)
#include <cmsis_os.h>
#define SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<cmsis_os2.h>)
#include <cmsis_os2.h>
#define SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<cmsis_os.h>)
#include <cmsis_os.h>
#define SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS 1
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
#if defined(SYSCAPE_CMSIS_RTOS_TOTAL_HEAP_SIZE) &&                             \
    (SYSCAPE_CMSIS_RTOS_TOTAL_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(SYSCAPE_CMSIS_RTOS_TOTAL_HEAP_SIZE);
#elif defined(CMSIS_RTOS_TOTAL_HEAP_SIZE) && (CMSIS_RTOS_TOTAL_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(CMSIS_RTOS_TOTAL_HEAP_SIZE);
#else
    return fail(errc::not_supported);
#endif
}

// Returns the available free heap memory in bytes on CMSIS-RTOS / Keil RTX.
// Note: Available memory of 0 bytes is a valid state (memory exhausted)
// and is not treated as an error sentinel. A guard of >= 0 ensures that
// negative values (such as -1 used as a sentinel) or unconfigured macros
// report not_supported.
inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_CMSIS_RTOS_FREE_HEAP_SIZE) &&                              \
    (SYSCAPE_CMSIS_RTOS_FREE_HEAP_SIZE >= 0)
    return static_cast<std::uint64_t>(SYSCAPE_CMSIS_RTOS_FREE_HEAP_SIZE);
#elif defined(CMSIS_RTOS_FREE_HEAP_SIZE) && (CMSIS_RTOS_FREE_HEAP_SIZE >= 0)
    return static_cast<std::uint64_t>(CMSIS_RTOS_FREE_HEAP_SIZE);
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

#if defined(SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS
#endif

#endif // SYSCAPE_DETAIL_MEMORY_CMSIS_RTOS_HPP
