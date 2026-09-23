#ifndef SYSCAPE_DETAIL_MEMORY_RIOT_HPP
#define SYSCAPE_DETAIL_MEMORY_RIOT_HPP

#include <syscape/detail/config.hpp>

#include <cstddef>
#include <cstdint>

#if !defined(SYSCAPE_RIOT_HAS_KERNEL_HEADERS)
#define SYSCAPE_RIOT_INTERNAL_KERNEL_HEADERS 1
#endif

#if defined(__has_include)
#if __has_include(<kernel_defines.h>)
#include <kernel_defines.h>
#define SYSCAPE_RIOT_HAS_KERNEL_HEADERS 1
#endif
#if __has_include(<cpu.h>)
#include <cpu.h>
#define SYSCAPE_RIOT_HAS_CPU 1
#endif
#if __has_include(<malloc_monitor.h>)
#include <malloc_monitor.h>
#define SYSCAPE_RIOT_HAS_MALLOC_MONITOR 1
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

// Returns the total physical RAM size in bytes if reported by target CPU/board
// facilities via cpu_get_ram_size(), or not_supported if hardware RAM capacity
// cannot be determined at runtime.
inline result<std::uint64_t> physical_memory_bytes() {
#if defined(SYSCAPE_RIOT_HAS_CPU) || defined(MODULE_CPU_RAM_SIZE) ||           \
    defined(RIOT_HAS_CPU_RAM_SIZE)
    const std::size_t ram = cpu_get_ram_size();
    if (ram > 0) {
        return static_cast<std::uint64_t>(ram);
    }
    return fail(errc::malformed_data);
#else
    return fail(errc::not_supported);
#endif
}

// Returns the available heap memory in bytes if the RIOT malloc_monitor module
// is enabled via get_mem_usage(), or not_supported if dynamic memory tracking
// is unavailable.
inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_RIOT_HAS_MALLOC_MONITOR) ||                                \
    defined(MODULE_MALLOC_MONITOR) || defined(RIOT_HAS_MALLOC_MONITOR)
    const std::size_t avail = get_mem_usage();
    return static_cast<std::uint64_t>(avail);
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

#if defined(SYSCAPE_RIOT_INTERNAL_KERNEL_HEADERS)
#undef SYSCAPE_RIOT_HAS_KERNEL_HEADERS
#undef SYSCAPE_RIOT_INTERNAL_KERNEL_HEADERS
#endif

#endif
