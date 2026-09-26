#ifndef SYSCAPE_DETAIL_MEMORY_RTTHREAD_HPP
#define SYSCAPE_DETAIL_MEMORY_RTTHREAD_HPP

#include <syscape/detail/config.hpp>

#include <cstddef>
#include <cstdint>

#if !defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
#define SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS 1
#endif

#if defined(__has_include)
#if __has_include(<rtthread.h>)
#include <rtthread.h>
#define SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS 1
#elif __has_include(<rtdef.h>)
#include <rtdef.h>
#define SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS 1
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

// Returns the total managed heap capacity in bytes on RT-Thread.
// When dynamic heap management is enabled, queries rt_memory_info();
// otherwise falls back to configured heap size macros.
inline result<std::uint64_t> physical_memory_bytes() {
#if defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
#if defined(RT_USING_HEAP) || defined(RT_USING_MEMHEAP) ||                     \
    defined(RT_USING_SMALL_MEM) || defined(RT_USING_SLAB)
    rt_size_t total = 0;
    rt_size_t used = 0;
    rt_size_t max_used = 0;
    rt_memory_info(&total, &used, &max_used);
    if (total > 0) {
        return static_cast<std::uint64_t>(total);
    }
#endif
#if defined(RT_TOTAL_HEAP_SIZE) && (RT_TOTAL_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(RT_TOTAL_HEAP_SIZE);
#elif defined(RT_HEAP_SIZE) && (RT_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(RT_HEAP_SIZE);
#else
    return fail(errc::not_supported);
#endif
#else
    return fail(errc::not_supported);
#endif
}

// Returns the available free heap memory in bytes on RT-Thread.
inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
#if defined(RT_USING_HEAP) || defined(RT_USING_MEMHEAP) ||                     \
    defined(RT_USING_SMALL_MEM) || defined(RT_USING_SLAB)
    rt_size_t total = 0;
    rt_size_t used = 0;
    rt_size_t max_used = 0;
    rt_memory_info(&total, &used, &max_used);
    if (total > 0) {
        if (used > total) {
            return fail(errc::malformed_data);
        }
        return static_cast<std::uint64_t>(total - used);
    }
#endif
#if defined(RT_FREE_HEAP_SIZE) && (RT_FREE_HEAP_SIZE > 0)
    return static_cast<std::uint64_t>(RT_FREE_HEAP_SIZE);
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

#if defined(SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS)
#undef SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS
#undef SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS
#endif

#endif
