#ifndef SYSCAPE_DETAIL_MEMORY_APPLE_MOBILE_HPP
#define SYSCAPE_DETAIL_MEMORY_APPLE_MOBILE_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <system_error>
#include <unistd.h>

#include <mach/host_info.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

/// Owns one Mach host send right for the duration of a single query.
class host_port {
    public:
    host_port() noexcept : value_(::mach_host_self()) {}
    host_port(const host_port&) = delete;
    host_port& operator=(const host_port&) = delete;
    ~host_port() {
        if (value_ != MACH_PORT_NULL) {
            ::mach_port_deallocate(::mach_task_self(), value_);
        }
    }

    /// Returns the owned port, or MACH_PORT_NULL after a failed acquisition.
    ::mach_port_t get() const noexcept {
        return value_;
    }

    private:
    ::mach_port_t value_;
};

/// Reads an integer sysctl value of exactly 64 bits.
inline result<std::uint64_t> sysctl_u64(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        return errno == ENOENT || errno == EOPNOTSUPP
                   ? result<std::uint64_t>(fail(errc::not_supported))
                   : result<std::uint64_t>(
                         fail(std::error_code(errno, std::generic_category())));
    }
    if (size != sizeof(std::uint64_t)) {
        return fail(errc::malformed_data);
    }
    std::uint64_t value = 0U;
    size = sizeof(value);
    if (::sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(std::uint64_t)) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Returns the Mach host page size in bytes without touching the caller's
/// host-port ownership.
inline result<std::uint64_t> query_page_size(::mach_port_t host) {
    ::vm_size_t page_size = 0U;
    const ::kern_return_t status = ::host_page_size(host, &page_size);
    if (status != KERN_SUCCESS) {
        return fail(errc::io_error);
    }
    if (page_size == 0U) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint64_t>(page_size);
}

inline result<std::uint64_t> page_size_bytes() {
    const host_port host;
    if (host.get() == MACH_PORT_NULL) {
        return fail(errc::io_error);
    }
    return query_page_size(host.get());
}

inline result<std::uint64_t> physical_memory_bytes() {
    const result<std::uint64_t> value = sysctl_u64("hw.memsize");
    if (!value) {
        return fail(value.error());
    }
    if (*value == 0U) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Queries the Mach host's 64-bit virtual-memory statistics into caller
/// storage.
inline result<void>
query_host_vm_statistics(::mach_port_t host,
                         ::vm_statistics64_data_t& statistics) {
    ::mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    const ::kern_return_t status = ::host_statistics64(
        host, HOST_VM_INFO64, reinterpret_cast<::host_info64_t>(&statistics),
        &count);
    if (status != KERN_SUCCESS) {
        return fail(errc::io_error);
    }
    if (count != HOST_VM_INFO64_COUNT) {
        return fail(errc::malformed_data);
    }
    return result<void>();
}

inline result<std::uint64_t> available_memory_bytes() {
    const host_port host;
    if (host.get() == MACH_PORT_NULL) {
        return fail(errc::io_error);
    }
    const result<std::uint64_t> page_size = query_page_size(host.get());
    if (!page_size) {
        return fail(page_size.error());
    }

    ::vm_statistics64_data_t statistics {};
    const result<void> queried =
        query_host_vm_statistics(host.get(), statistics);
    if (!queried) {
        return fail(queried.error());
    }

    const result<std::uint64_t> physical = physical_memory_bytes();
    if (!physical) {
        return fail(physical.error());
    }

    const std::uint64_t pages =
        static_cast<std::uint64_t>(statistics.free_count) +
        static_cast<std::uint64_t>(statistics.inactive_count);
    if (*page_size != 0U &&
        pages > (std::numeric_limits<std::uint64_t>::max)() / *page_size) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t available = pages * *page_size;
    if (available > *physical) {
        return fail(errc::malformed_data);
    }
    return available;
}

inline result<std::uint32_t> memory_load_percent() {
    const host_port host;
    if (host.get() == MACH_PORT_NULL) {
        return fail(errc::io_error);
    }
    const result<std::uint64_t> page_size = query_page_size(host.get());
    if (!page_size) {
        return fail(page_size.error());
    }

    ::vm_statistics64_data_t statistics {};
    const result<void> queried =
        query_host_vm_statistics(host.get(), statistics);
    if (!queried) {
        return fail(queried.error());
    }

    const result<std::uint64_t> physical = sysctl_u64("hw.memsize");
    if (!physical) {
        return fail(physical.error());
    }
    if (*physical == 0U) {
        return fail(errc::malformed_data);
    }
    if (*physical < *page_size) {
        return fail(errc::malformed_data);
    }

    const std::uint64_t available_pages =
        static_cast<std::uint64_t>(statistics.free_count) +
        static_cast<std::uint64_t>(statistics.inactive_count);
    if (available_pages > *physical / *page_size) {
        return fail(errc::malformed_data);
    }
    return memory_common::utilization_percent(
        *physical - available_pages * *page_size, *physical);
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

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

inline result<memory_common::swap_usage> swap_status() {
    std::size_t size = 0U;
    if (::sysctlbyname("vm.swapusage", nullptr, &size, nullptr, 0U) != 0) {
        return errno == ENOENT || errno == EOPNOTSUPP
                   ? result<memory_common::swap_usage>(
                         fail(errc::not_supported))
                   : result<memory_common::swap_usage>(
                         fail(std::error_code(errno, std::generic_category())));
    }
    if (size != sizeof(::xsw_usage)) {
        return fail(errc::malformed_data);
    }
    ::xsw_usage native {};
    size = sizeof(native);
    if (::sysctlbyname("vm.swapusage", &native, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(::xsw_usage)) {
        return fail(errc::malformed_data);
    }
    memory_common::swap_usage usage;
    usage.total_bytes = static_cast<std::uint64_t>(native.xsu_total);
    usage.free_bytes = static_cast<std::uint64_t>(native.xsu_avail);
    return usage;
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
