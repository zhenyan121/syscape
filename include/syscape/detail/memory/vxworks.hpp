#ifndef SYSCAPE_DETAIL_MEMORY_VXWORKS_HPP
#define SYSCAPE_DETAIL_MEMORY_VXWORKS_HPP

#include <cerrno>
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
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    return fail(errc::not_supported);
#elif defined(_SC_PAGESIZE)
    errno = 0;
    const long size = ::sysconf(_SC_PAGESIZE);
    if (size > 0) {
        return static_cast<std::uint64_t>(size);
    }
    const int err = errno;
    if (err == EINVAL) {
        return fail(errc::not_supported);
    }
    if (err != 0) {
        return fail(std::error_code(err, std::generic_category()));
    }
    return fail(errc::malformed_data);
#elif defined(_SC_PAGE_SIZE)
    errno = 0;
    const long size = ::sysconf(_SC_PAGE_SIZE);
    if (size > 0) {
        return static_cast<std::uint64_t>(size);
    }
    const int err = errno;
    if (err == EINVAL) {
        return fail(errc::not_supported);
    }
    if (err != 0) {
        return fail(std::error_code(err, std::generic_category()));
    }
    return fail(errc::malformed_data);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> physical_memory_bytes() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> available_memory_bytes() {
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

#endif
