#ifndef SYSCAPE_DETAIL_MEMORY_AIX_HPP
#define SYSCAPE_DETAIL_MEMORY_AIX_HPP

#include <cstdint>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<libperfstat.h>)
#include <libperfstat.h>
#define SYSCAPE_HAS_AIX_LIBPERFSTAT 1
#endif
#endif

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
    const long ps = ::sysconf(_SC_PAGESIZE);
    if (ps > 0) {
        return static_cast<std::uint64_t>(ps);
    }
    return fail(errc::not_supported);
}

inline result<std::uint64_t> physical_memory_bytes() {
#if defined(SYSCAPE_HAS_AIX_LIBPERFSTAT)
    perfstat_memory_total_t mem {};
    if (::perfstat_memory_total(nullptr, &mem, sizeof(perfstat_memory_total_t),
                                1) > 0) {
        return static_cast<std::uint64_t>(mem.real_total) * 4096ULL;
    }
#endif
#if defined(_SC_PHYS_PAGES)
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    const auto ps = page_size_bytes();
    if (pages > 0 && ps) {
        return static_cast<std::uint64_t>(pages) * (*ps);
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_HAS_AIX_LIBPERFSTAT)
    perfstat_memory_total_t mem {};
    if (::perfstat_memory_total(nullptr, &mem, sizeof(perfstat_memory_total_t),
                                1) > 0) {
        return static_cast<std::uint64_t>(mem.real_free) * 4096ULL;
    }
#endif
#if defined(_SC_AVPHYS_PAGES)
    const long pages = ::sysconf(_SC_AVPHYS_PAGES);
    const auto ps = page_size_bytes();
    if (pages > 0 && ps) {
        return static_cast<std::uint64_t>(pages) * (*ps);
    }
#endif
    return fail(errc::not_supported);
}

inline result<memory_common::swap_usage> swap_status() {
#if defined(SYSCAPE_HAS_AIX_LIBPERFSTAT)
    perfstat_memory_total_t mem {};
    if (::perfstat_memory_total(nullptr, &mem, sizeof(perfstat_memory_total_t),
                                1) > 0) {
        memory_common::swap_usage usage {};
        usage.total_bytes =
            static_cast<std::uint64_t>(mem.pgsp_total) * 4096ULL;
        usage.free_bytes = static_cast<std::uint64_t>(mem.pgsp_free) * 4096ULL;
        return usage;
    }
#endif
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
    const auto avail = available_memory_bytes();
    if (total && avail && *total > 0 && *avail <= *total) {
        const std::uint64_t used = *total - *avail;
        return static_cast<std::uint32_t>((used * 100ULL) / *total);
    }
    return fail(errc::not_supported);
}

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
