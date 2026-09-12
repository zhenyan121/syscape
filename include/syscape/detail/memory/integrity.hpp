#ifndef SYSCAPE_DETAIL_MEMORY_INTEGRITY_HPP
#define SYSCAPE_DETAIL_MEMORY_INTEGRITY_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<INTEGRITY.h>)
#include <INTEGRITY.h>
#define SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS 1
#elif __has_include(<integrity.h>)
#include <integrity.h>
#define SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS 1
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
#if defined(INTEGRITY_TOTAL_HEAP_SIZE)
    return static_cast<std::uint64_t>(INTEGRITY_TOTAL_HEAP_SIZE);
#elif defined(INTEGRITY_HEAP_SIZE)
    return static_cast<std::uint64_t>(INTEGRITY_HEAP_SIZE);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS)
    return static_cast<std::uint64_t>(::GetMemoryPoolFreeSpace());
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

#if defined(SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS)
#undef SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS
#endif

#endif
