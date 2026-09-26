#ifndef SYSCAPE_DETAIL_MEMORY_SAILFISH_HPP
#define SYSCAPE_DETAIL_MEMORY_SAILFISH_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstdint>
#include <system_error>
#include <type_traits>
#include <unistd.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

template <typename T>
inline result<std::uint64_t> validate_positive_u64_bytes(T value) {
    if constexpr (std::is_signed<T>::value) {
        if (value <= 0) {
            return fail(errc::malformed_data);
        }
    } else {
        if (value == 0) {
            return fail(errc::malformed_data);
        }
    }
    return static_cast<std::uint64_t>(value);
}

template <typename T>
inline result<std::uint64_t> validate_non_negative_u64_bytes(T value) {
    if constexpr (std::is_signed<T>::value) {
        if (value < 0) {
            return fail(errc::malformed_data);
        }
    }
    return static_cast<std::uint64_t>(value);
}

/// Returns the system page size via override or POSIX sysconf.
inline result<std::uint64_t> page_size_bytes() {
#if defined(SYSCAPE_SAILFISH_PAGE_SIZE_BYTES)
    return validate_positive_u64_bytes(SYSCAPE_SAILFISH_PAGE_SIZE_BYTES);
#else
    errno = 0;
    const long value = ::sysconf(_SC_PAGESIZE);
    if (value <= 0) {
        return errno != 0
                   ? result<std::uint64_t>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<std::uint64_t>(fail(errc::not_supported));
    }
    return static_cast<std::uint64_t>(value);
#endif
}

/// Returns the total physical RAM bytes if supplied by override macro.
inline result<std::uint64_t> physical_memory_bytes() {
#if defined(SYSCAPE_SAILFISH_TOTAL_RAM_BYTES)
    return validate_positive_u64_bytes(SYSCAPE_SAILFISH_TOTAL_RAM_BYTES);
#elif defined(SYSCAPE_SAILFISH_PHYSICAL_MEMORY_BYTES)
    return validate_positive_u64_bytes(SYSCAPE_SAILFISH_PHYSICAL_MEMORY_BYTES);
#else
    return fail(errc::not_supported);
#endif
}

/// Returns available free RAM bytes if supplied by override macro.
inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_SAILFISH_FREE_RAM_BYTES)
    return validate_non_negative_u64_bytes(SYSCAPE_SAILFISH_FREE_RAM_BYTES);
#elif defined(SYSCAPE_SAILFISH_AVAILABLE_MEMORY_BYTES)
    return validate_non_negative_u64_bytes(
        SYSCAPE_SAILFISH_AVAILABLE_MEMORY_BYTES);
#else
    return fail(errc::not_supported);
#endif
}

/// Swap status is not accessible in unprivileged application sandboxes.
inline result<memory_common::swap_usage> swap_status() {
    return fail(errc::not_supported);
}

/// Commit status is not accessible in unprivileged application sandboxes.
inline result<memory_common::commit_usage> commit_status() {
    return fail(errc::not_supported);
}

/// Huge page size is not exposed in unprivileged application sandboxes.
inline result<std::uint64_t> huge_page_size_bytes() {
    return fail(errc::not_supported);
}

/// Huge page pool status is not exposed in unprivileged application sandboxes.
inline result<memory_common::huge_page_pool_usage> huge_page_pool_status() {
    return fail(errc::not_supported);
}

/// Computes memory load percent from physical and available memory snapshots.
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

/// Memory pressure notifications require privileged kernel cgroup interfaces.
inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
