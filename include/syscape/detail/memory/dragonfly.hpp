#ifndef SYSCAPE_DETAIL_MEMORY_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_MEMORY_DRAGONFLY_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <system_error>
#include <unistd.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <vm/vm_param.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> sysctl_u64(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == sizeof(std::uint32_t)) {
        std::uint32_t val = 0;
        if (::sysctlbyname(name, &val, &size, nullptr, 0U) != 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        return static_cast<std::uint64_t>(val);
    }
    if (size == sizeof(std::uint64_t)) {
        std::uint64_t val = 0;
        if (::sysctlbyname(name, &val, &size, nullptr, 0U) != 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        return val;
    }
    if (size == sizeof(unsigned long)) {
        unsigned long val = 0;
        if (::sysctlbyname(name, &val, &size, nullptr, 0U) != 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        return static_cast<std::uint64_t>(val);
    }
    return fail(errc::malformed_data);
}

inline result<std::uint64_t> page_size_bytes() {
    errno = 0;
    const long value = ::sysconf(_SC_PAGESIZE);
    if (value <= 0) {
        return errno != 0
                   ? result<std::uint64_t>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<std::uint64_t>(fail(errc::malformed_data));
    }
    return static_cast<std::uint64_t>(value);
}

inline result<std::uint64_t> physical_memory_bytes() {
    const result<std::uint64_t> value = sysctl_u64("hw.physmem");
    if (!value) {
        return fail(value.error());
    }
    if (*value == 0U) {
        return fail(errc::malformed_data);
    }
    return value;
}

inline result<std::uint64_t> available_memory_bytes() {
    const result<std::uint64_t> page_size = page_size_bytes();
    if (!page_size) {
        return fail(page_size.error());
    }

    const auto free_res = sysctl_u64("vm.stats.vm.v_free_count");
    if (!free_res) {
        return fail(free_res.error());
    }
    const auto inactive_res = sysctl_u64("vm.stats.vm.v_inactive_count");
    if (!inactive_res) {
        return fail(inactive_res.error());
    }
    const auto cache_res = sysctl_u64("vm.stats.vm.v_cache_count");
    std::uint64_t cache_pages = 0U;
    if (cache_res) {
        cache_pages = *cache_res;
    } else if (cache_res.error() != errc::not_supported &&
               cache_res.error() != std::errc::no_such_file_or_directory) {
        return fail(cache_res.error());
    }

    constexpr std::uint64_t max_u64 =
        (std::numeric_limits<std::uint64_t>::max)();
    if (*free_res > max_u64 - *inactive_res) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t free_plus_inactive = *free_res + *inactive_res;
    if (free_plus_inactive > max_u64 - cache_pages) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t total_avail_pages = free_plus_inactive + cache_pages;

    if (*page_size != 0U && total_avail_pages > max_u64 / *page_size) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t available_bytes = total_avail_pages * *page_size;

    const result<std::uint64_t> physical = physical_memory_bytes();
    if (!physical) {
        return fail(physical.error());
    }
    if (available_bytes > *physical) {
        return fail(errc::malformed_data);
    }

    return available_bytes;
}

inline result<memory_common::swap_usage> swap_status() {
    auto total_res = sysctl_u64("vm.swap_size");
    if (!total_res) {
        if (total_res.error() == errc::not_supported ||
            total_res.error() == std::errc::no_such_file_or_directory) {
            total_res = sysctl_u64("vm.swap_total");
        } else {
            return fail(total_res.error());
        }
    }
    if (!total_res) {
        return fail(total_res.error());
    }
    if (*total_res == 0U) {
        memory_common::swap_usage usage;
        usage.total_bytes = 0U;
        usage.free_bytes = 0U;
        return usage;
    }
    const result<std::uint64_t> page_size = page_size_bytes();
    if (!page_size) {
        return fail(page_size.error());
    }
    constexpr std::uint64_t max_u64 =
        (std::numeric_limits<std::uint64_t>::max)();
    if (*total_res > max_u64 / *page_size) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t total_bytes = *total_res * *page_size;

    const auto free_res = sysctl_u64("vm.swap_free");
    if (free_res) {
        if (*free_res > max_u64 / *page_size) {
            return fail(errc::value_too_large);
        }
        std::uint64_t free_bytes = *free_res * *page_size;
        if (free_bytes > total_bytes) {
            free_bytes = total_bytes;
        }
        memory_common::swap_usage usage;
        usage.total_bytes = total_bytes;
        usage.free_bytes = free_bytes;
        return usage;
    }
    if (free_res.error() != errc::not_supported &&
        free_res.error() != std::errc::no_such_file_or_directory) {
        return fail(free_res.error());
    }

    const auto anon_res = sysctl_u64("vm.swap_anon_use");
    if (!anon_res && anon_res.error() != errc::not_supported &&
        anon_res.error() != std::errc::no_such_file_or_directory) {
        return fail(anon_res.error());
    }

    const auto cache_res = sysctl_u64("vm.swap_cache_use");
    if (!cache_res && cache_res.error() != errc::not_supported &&
        cache_res.error() != std::errc::no_such_file_or_directory) {
        return fail(cache_res.error());
    }

    if (!anon_res && !cache_res) {
        return fail(errc::not_supported);
    }

    std::uint64_t used_pages = 0U;
    if (anon_res) {
        if (*anon_res > max_u64 - used_pages) {
            return fail(errc::value_too_large);
        }
        used_pages += *anon_res;
    }
    if (cache_res) {
        if (*cache_res > max_u64 - used_pages) {
            return fail(errc::value_too_large);
        }
        used_pages += *cache_res;
    }
    if (used_pages > max_u64 / *page_size) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t used_bytes = used_pages * *page_size;
    memory_common::swap_usage usage;
    usage.total_bytes = total_bytes;
    usage.free_bytes =
        (total_bytes >= used_bytes) ? (total_bytes - used_bytes) : 0U;
    return usage;
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
    const result<std::uint64_t> physical = physical_memory_bytes();
    if (!physical) {
        return fail(physical.error());
    }
    const result<std::uint64_t> available = available_memory_bytes();
    if (!available) {
        return fail(available.error());
    }
    if (*available > *physical) {
        return fail(errc::malformed_data);
    }
    return memory_common::utilization_percent(*physical - *available,
                                              *physical);
}

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
