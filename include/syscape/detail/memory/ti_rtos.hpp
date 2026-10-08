#ifndef SYSCAPE_DETAIL_MEMORY_TI_RTOS_HPP
#define SYSCAPE_DETAIL_MEMORY_TI_RTOS_HPP

/// @file
/// @brief TI-RTOS (SYS/BIOS) backend implementation for memory queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses SYSCAPE_TI_RTOS_TOTAL_HEAP_SIZE/FREE_HEAP_SIZE or
/// TI_RTOS_TOTAL_HEAP_SIZE/FREE_HEAP_SIZE
///       configuration macros for heap statistics.

#include <syscape/detail/config.hpp>

#include <cstddef>
#include <cstdint>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> physical_memory_bytes() {
#if defined(SYSCAPE_TI_RTOS_TOTAL_HEAP_SIZE) &&                                \
    (SYSCAPE_TI_RTOS_TOTAL_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(SYSCAPE_TI_RTOS_TOTAL_HEAP_SIZE);
#elif defined(TI_RTOS_TOTAL_HEAP_SIZE) && (TI_RTOS_TOTAL_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(TI_RTOS_TOTAL_HEAP_SIZE);
#else
    return fail(errc::not_supported);
#endif
}

// Returns the available free heap memory in bytes on TI-RTOS (SYS/BIOS).
// Note: Available memory of 0 bytes is a valid state (memory exhausted)
// and is not treated as an error sentinel. A guard of >= 0 ensures that
// negative values (such as -1 used as a sentinel) or unconfigured macros
// report not_supported.
inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_TI_RTOS_FREE_HEAP_SIZE) &&                                 \
    (SYSCAPE_TI_RTOS_FREE_HEAP_SIZE >= 0)
    static_assert(SYSCAPE_TI_RTOS_FREE_HEAP_SIZE >= 0,
                  "SYSCAPE_TI_RTOS_FREE_HEAP_SIZE must not be negative");
    return static_cast<std::uint64_t>(SYSCAPE_TI_RTOS_FREE_HEAP_SIZE);
#elif defined(TI_RTOS_FREE_HEAP_SIZE) && (TI_RTOS_FREE_HEAP_SIZE >= 0)
    static_assert(TI_RTOS_FREE_HEAP_SIZE >= 0,
                  "TI_RTOS_FREE_HEAP_SIZE must not be negative");
    return static_cast<std::uint64_t>(TI_RTOS_FREE_HEAP_SIZE);
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

#endif // SYSCAPE_DETAIL_MEMORY_TI_RTOS_HPP
