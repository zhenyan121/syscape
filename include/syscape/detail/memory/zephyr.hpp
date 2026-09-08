#ifndef SYSCAPE_DETAIL_MEMORY_ZEPHYR_HPP
#define SYSCAPE_DETAIL_MEMORY_ZEPHYR_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstdint>
#include <limits>
#include <system_error>

#if defined(SYSCAPE_ZEPHYR_HAS_POSIX_SINGLE_PROCESS)
#include <unistd.h>
#endif

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
#if defined(SYSCAPE_ZEPHYR_HAS_POSIX_SINGLE_PROCESS)
    errno = 0;
    const long size = sysconf(_SC_PAGESIZE);
    if (size > 0) {
        return static_cast<std::uint64_t>(size);
    }
    if (size == 0) {
        return fail(errc::malformed_data);
    }
    const int error = errno;
    if (error != 0) {
        return fail(std::error_code(error, std::generic_category()));
    }
    return fail(errc::not_supported);
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
