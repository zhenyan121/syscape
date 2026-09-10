#ifndef SYSCAPE_DETAIL_MEMORY_EMSCRIPTEN_HPP
#define SYSCAPE_DETAIL_MEMORY_EMSCRIPTEN_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstdint>
#include <system_error>
#include <unistd.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
#if defined(_SC_PAGESIZE) || defined(_SC_PAGE_SIZE)
    errno = 0;
#if defined(_SC_PAGESIZE)
    const long size = ::sysconf(_SC_PAGESIZE);
#else
    const long size = ::sysconf(_SC_PAGE_SIZE);
#endif
    if (size > 0) {
        const auto value = static_cast<std::uint64_t>(size);
        return (value & (value - 1U)) == 0U ? result<std::uint64_t>(value)
                                            : fail(errc::malformed_data);
    }
    if (size == 0) {
        return fail(errc::malformed_data);
    }
    const int error = errno;
    if (error == EACCES || error == EPERM) {
        return fail(errc::permission_denied);
    }
    if (error != 0 && error != EINVAL && error != ENOSYS && error != ENOTSUP) {
        return fail(std::error_code(error, std::generic_category()));
    }
#endif
    return fail(errc::not_supported);
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
