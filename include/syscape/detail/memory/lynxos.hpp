#ifndef SYSCAPE_DETAIL_MEMORY_LYNXOS_HPP
#define SYSCAPE_DETAIL_MEMORY_LYNXOS_HPP

/// @file
/// @brief LynxOS / LynxOS-178 RTOS backend implementation for memory queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (RTOS/Constrained profile).
/// @note Minimum language version: C++17.
/// @note Uses sysconf(_SC_PAGESIZE), sysconf(_SC_PHYS_PAGES), and
/// sysconf(_SC_AVPHYS_PAGES).

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <system_error>
#include <unistd.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
#if defined(_SC_PAGESIZE)
    errno = 0;
    const long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size > 0) {
        return static_cast<std::uint64_t>(page_size);
    }
    if (page_size == 0) {
        return fail(errc::malformed_data);
    }
    const int error = errno;
    if (error != 0 && error != EINVAL) {
        return fail(std::error_code(error, std::generic_category()));
    }
#elif defined(_SC_PAGE_SIZE)
    errno = 0;
    const long page_size = ::sysconf(_SC_PAGE_SIZE);
    if (page_size > 0) {
        return static_cast<std::uint64_t>(page_size);
    }
    if (page_size == 0) {
        return fail(errc::malformed_data);
    }
    const int error = errno;
    if (error != 0 && error != EINVAL) {
        return fail(std::error_code(error, std::generic_category()));
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint64_t> physical_memory_bytes() {
#if defined(_SC_PHYS_PAGES)
    const auto page = page_size_bytes();
    if (!page) {
        return fail(page.error());
    }
    errno = 0;
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    if (pages > 0) {
        if (static_cast<std::uint64_t>(pages) >
            (std::numeric_limits<std::uint64_t>::max)() / *page) {
            return fail(errc::value_too_large);
        }
        return static_cast<std::uint64_t>(pages) * *page;
    }
    if (pages == 0) {
        return fail(errc::malformed_data);
    }
    const int error = errno;
    if (error != 0 && error != EINVAL) {
        return fail(std::error_code(error, std::generic_category()));
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(_SC_AVPHYS_PAGES)
    const auto page = page_size_bytes();
    if (!page) {
        return fail(page.error());
    }
    errno = 0;
    const long pages = ::sysconf(_SC_AVPHYS_PAGES);
    if (pages >= 0) {
        if (pages == 0) {
            return static_cast<std::uint64_t>(0U);
        }
        if (static_cast<std::uint64_t>(pages) >
            (std::numeric_limits<std::uint64_t>::max)() / *page) {
            return fail(errc::value_too_large);
        }
        return static_cast<std::uint64_t>(pages) * *page;
    }
    const int error = errno;
    if (error != 0 && error != EINVAL) {
        return fail(std::error_code(error, std::generic_category()));
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
