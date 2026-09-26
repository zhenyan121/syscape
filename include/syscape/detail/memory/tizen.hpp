#ifndef SYSCAPE_DETAIL_MEMORY_TIZEN_HPP
#define SYSCAPE_DETAIL_MEMORY_TIZEN_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
#if defined(SYSCAPE_TIZEN_PAGE_SIZE_BYTES)
    return static_cast<std::uint64_t>(SYSCAPE_TIZEN_PAGE_SIZE_BYTES);
#elif defined(__arm__) || defined(__aarch64__) || defined(__i386__) ||         \
    defined(__x86_64__) || defined(_M_ARM) || defined(_M_ARM64) ||             \
    defined(_M_IX86) || defined(_M_X64)
    return static_cast<std::uint64_t>(4096U);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> physical_memory_bytes() {
#if defined(SYSCAPE_TIZEN_TOTAL_RAM_BYTES)
    return static_cast<std::uint64_t>(SYSCAPE_TIZEN_TOTAL_RAM_BYTES);
#elif defined(SYSCAPE_TIZEN_PHYSICAL_MEMORY_BYTES)
    return static_cast<std::uint64_t>(SYSCAPE_TIZEN_PHYSICAL_MEMORY_BYTES);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_TIZEN_FREE_RAM_BYTES)
    return static_cast<std::uint64_t>(SYSCAPE_TIZEN_FREE_RAM_BYTES);
#elif defined(SYSCAPE_TIZEN_AVAILABLE_MEMORY_BYTES)
    return static_cast<std::uint64_t>(SYSCAPE_TIZEN_AVAILABLE_MEMORY_BYTES);
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
    const auto total = physical_memory_bytes();
    if (!total) {
        return fail(total.error());
    }
    const auto available = available_memory_bytes();
    if (!available) {
        return fail(available.error());
    }
    if (*available > *total) {
        return fail(errc::malformed_data);
    }
    return memory_common::utilization_percent(*total - *available, *total);
}

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
