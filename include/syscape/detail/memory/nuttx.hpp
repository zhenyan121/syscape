#ifndef SYSCAPE_DETAIL_MEMORY_NUTTX_HPP
#define SYSCAPE_DETAIL_MEMORY_NUTTX_HPP

#include <cerrno>
#include <cstdint>
#include <limits>
#include <system_error>
#include <unistd.h>
#include <sys/sysinfo.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> nuttx_scaled_memory(unsigned long amount,
                                                 unsigned int unit) {
    if (unit == 0U) {
        return fail(errc::malformed_data);
    }
    const auto value = static_cast<std::uint64_t>(amount);
    const auto scale = static_cast<std::uint64_t>(unit);
    if (value > (std::numeric_limits<std::uint64_t>::max)() / scale) {
        return fail(errc::value_too_large);
    }
    return value * scale;
}

inline result<struct ::sysinfo> nuttx_memory_info() {
    struct ::sysinfo value {};
    errno = 0;
    if (::sysinfo(&value) == 0) {
        return value;
    }
    const int error = errno;
    if (error == ENOSYS || error == EINVAL) {
        return fail(errc::not_supported);
    }
    return error != 0 ? fail(std::error_code(error, std::generic_category()))
                      : fail(errc::io_error);
}

inline result<std::uint64_t> page_size_bytes() {
    errno = 0;
    const long size = ::sysconf(_SC_PAGESIZE);
    if (size > 0) {
        const auto value = static_cast<std::uint64_t>(size);
        return (value & (value - 1U)) == 0U ? result<std::uint64_t>(value)
                                            : fail(errc::malformed_data);
    }
    if (size == 0) {
        return fail(errc::malformed_data);
    }
    const int error = errno;
    if (error == EINVAL || error == ENOSYS || error == 0) {
        return fail(errc::not_supported);
    }
    return fail(std::error_code(error, std::generic_category()));
}

inline result<std::uint64_t> physical_memory_bytes() {
    const auto info = nuttx_memory_info();
    if (!info) {
        return fail(info.error());
    }
    const auto total = nuttx_scaled_memory(info->totalram, info->mem_unit);
    if (!total) {
        return fail(total.error());
    }
    return *total != 0U ? total : fail(errc::not_found);
}

inline result<std::uint64_t> available_memory_bytes() {
    const auto info = nuttx_memory_info();
    if (!info) {
        return fail(info.error());
    }
    const auto total = nuttx_scaled_memory(info->totalram, info->mem_unit);
    const auto free = nuttx_scaled_memory(info->freeram, info->mem_unit);
    if (!total || !free) {
        return fail(!total ? total.error() : free.error());
    }
    if (*total == 0U) {
        return fail(errc::not_found);
    }
    if (*free > *total) {
        return fail(errc::malformed_data);
    }
    return free;
}

inline result<memory_common::swap_usage> swap_status() {
    // NuttX sysinfo currently leaves both swap fields zero-filled rather than
    // reporting a configured swap facility.
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
    const auto info = nuttx_memory_info();
    if (!info) {
        return fail(info.error());
    }
    const auto total = nuttx_scaled_memory(info->totalram, info->mem_unit);
    const auto free = nuttx_scaled_memory(info->freeram, info->mem_unit);
    if (!total || !free) {
        return fail(!total ? total.error() : free.error());
    }
    if (*total == 0U) {
        return fail(errc::not_found);
    }
    if (*free > *total) {
        return fail(errc::malformed_data);
    }
    return memory_common::utilization_percent(*total - *free, *total);
}

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
