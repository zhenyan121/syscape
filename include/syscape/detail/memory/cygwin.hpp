#ifndef SYSCAPE_DETAIL_MEMORY_CYGWIN_HPP
#define SYSCAPE_DETAIL_MEMORY_CYGWIN_HPP

#include <cstdint>
#include <unistd.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
    const int page_size = ::getpagesize();
    if (page_size <= 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint64_t>(page_size);
}

inline result<std::uint64_t> physical_memory_bytes() {
#if defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    const long page_size = ::sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_size > 0) {
        return static_cast<std::uint64_t>(pages) *
               static_cast<std::uint64_t>(page_size);
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(_SC_AVPHYS_PAGES) && defined(_SC_PAGESIZE)
    const long pages = ::sysconf(_SC_AVPHYS_PAGES);
    const long page_size = ::sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_size > 0) {
        return static_cast<std::uint64_t>(pages) *
               static_cast<std::uint64_t>(page_size);
    }
#endif
    return fail(errc::not_supported);
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
    return memory_common::utilization_percent(*total, *available);
}

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
