#ifndef SYSCAPE_DETAIL_MEMORY_CHIBIOS_HPP
#define SYSCAPE_DETAIL_MEMORY_CHIBIOS_HPP

#include <syscape/detail/config.hpp>

#include <cstddef>
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

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> physical_memory_bytes() {
#if defined(CH_TOTAL_HEAP_SIZE) && (CH_TOTAL_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(CH_TOTAL_HEAP_SIZE);
#elif defined(CH_HEAP_SIZE) && (CH_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(CH_HEAP_SIZE);
#elif defined(CH_CFG_MEMCORE_SIZE) && (CH_CFG_MEMCORE_SIZE > 0)
    return static_cast<std::uint64_t>(CH_CFG_MEMCORE_SIZE);
#elif defined(CH_MEMCORE_SIZE) && (CH_MEMCORE_SIZE > 0)
    return static_cast<std::uint64_t>(CH_MEMCORE_SIZE);
#elif defined(CH_CORE_SIZE) && (CH_CORE_SIZE > 0)
    return static_cast<std::uint64_t>(CH_CORE_SIZE);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS)
#if (defined(CH_CFG_USE_HEAP) && (CH_CFG_USE_HEAP == 0)) ||                    \
    (defined(CH_USE_HEAP) && (CH_USE_HEAP == 0))
    return fail(errc::not_supported);
#else
    std::size_t total_free = 0;
    std::size_t largest_free = 0;
    chHeapStatus(nullptr, &total_free, &largest_free);
    return static_cast<std::uint64_t>(total_free);
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

#if defined(SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_CHIBIOS_HAS_KERNEL_HEADERS
#endif

#endif
