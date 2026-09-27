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

// Returns the total managed heap capacity in bytes on RT-Thread via
// rt_memory_info() when dynamic heap management (RT_USING_HEAP) is enabled.
// Note: This reports the kernel-managed heap pool rather than total hardware
// RAM.
inline result<std::uint64_t> physical_memory_bytes() {
#if defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
#if defined(RT_USING_HEAP)
    rt_size_t total = 0;
    rt_size_t used = 0;
    rt_size_t max_used = 0;
    rt_memory_info(&total, &used, &max_used);
    if (total > 0) {
        return static_cast<std::uint64_t>(total);
    }
#endif
    return fail(errc::not_supported);
#else
    return fail(errc::not_supported);
#endif
}

// Returns the available free heap memory in bytes on RT-Thread.
inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
#if defined(RT_USING_HEAP)
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

#if defined(SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS)
#undef SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS
#undef SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS
#endif

#endif
