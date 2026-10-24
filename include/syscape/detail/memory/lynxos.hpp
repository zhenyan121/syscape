#ifndef SYSCAPE_DETAIL_MEMORY_LYNXOS_HPP
#define SYSCAPE_DETAIL_MEMORY_LYNXOS_HPP

/// @file
/// @brief LynxOS / LynxOS-178 RTOS backend implementation for memory queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses sysconf(_SC_PAGESIZE) and
/// SYSCAPE_LYNXOS_TOTAL_HEAP_SIZE/FREE_HEAP_SIZE macros.

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <unistd.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
#if defined(_SC_PAGESIZE)
    const long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size > 0) {
        return static_cast<std::uint64_t>(page_size);
    }
#elif defined(_SC_PAGE_SIZE)
    const long page_size = ::sysconf(_SC_PAGE_SIZE);
    if (page_size > 0) {
        return static_cast<std::uint64_t>(page_size);
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint64_t> physical_memory_bytes() {
#if defined(SYSCAPE_LYNXOS_TOTAL_HEAP_SIZE)
    static_assert(SYSCAPE_LYNXOS_TOTAL_HEAP_SIZE > 0,
                  "SYSCAPE_LYNXOS_TOTAL_HEAP_SIZE must be positive");
    return static_cast<std::uint64_t>(SYSCAPE_LYNXOS_TOTAL_HEAP_SIZE);
#elif defined(LYNXOS_TOTAL_HEAP_SIZE)
    static_assert(LYNXOS_TOTAL_HEAP_SIZE > 0,
                  "LYNXOS_TOTAL_HEAP_SIZE must be positive");
    return static_cast<std::uint64_t>(LYNXOS_TOTAL_HEAP_SIZE);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_LYNXOS_FREE_HEAP_SIZE)
    static_assert(SYSCAPE_LYNXOS_FREE_HEAP_SIZE >= 0,
                  "SYSCAPE_LYNXOS_FREE_HEAP_SIZE must not be negative");
    return static_cast<std::uint64_t>(SYSCAPE_LYNXOS_FREE_HEAP_SIZE);
#elif defined(LYNXOS_FREE_HEAP_SIZE)
    static_assert(LYNXOS_FREE_HEAP_SIZE >= 0,
                  "LYNXOS_FREE_HEAP_SIZE must not be negative");
    return static_cast<std::uint64_t>(LYNXOS_FREE_HEAP_SIZE);
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

#endif // SYSCAPE_DETAIL_MEMORY_LYNXOS_HPP
