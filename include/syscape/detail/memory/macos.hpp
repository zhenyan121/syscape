#ifndef SYSCAPE_DETAIL_MEMORY_MACOS_HPP
#define SYSCAPE_DETAIL_MEMORY_MACOS_HPP

#include <cerrno>
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
    ::mach_port_t get() const noexcept { return value_; }

private:
    ::mach_port_t value_;
};

/// Reads an integer sysctl value of exactly 64 bits.
inline result<std::uint64_t> sysctl_u64(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(std::uint64_t)) { return fail(errc::malformed_data); }
    std::uint64_t value = 0U;
    if (::sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return value;
}

/// Returns the Mach host page size in bytes without touching the caller's
/// host-port ownership.
inline result<std::uint64_t> query_page_size(::mach_port_t host) {
    ::vm_size_t page_size = 0U;
    const ::kern_return_t status = ::host_page_size(host, &page_size);
    if (status != KERN_SUCCESS) { return fail(errc::io_error); }
    if (page_size == 0U) { return fail(errc::malformed_data); }
    return static_cast<std::uint64_t>(page_size);
}

inline result<std::uint64_t> page_size_bytes() {
    const host_port host;
    if (host.get() == MACH_PORT_NULL) { return fail(errc::io_error); }
    return query_page_size(host.get());
}

inline result<std::uint64_t> physical_memory_bytes() {
    const result<std::uint64_t> value = sysctl_u64("hw.memsize");
    if (!value) { return fail(value.error()); }
    if (*value == 0U) { return fail(errc::malformed_data); }
    return value;
}

/// Returns free and inactive pages as the operating system's estimate of
/// allocatable-without-swapping memory.
///
/// Volatile purgeable pages already reside in the kernel's inactive
/// population, so purgeable_count is deliberately not added a second time.
/// The Mach statistics call cannot preserve its kern_return_t code through a
/// standard error category, so failures map to io_error.
inline result<std::uint64_t> available_memory_bytes() {
    const host_port host;
    if (host.get() == MACH_PORT_NULL) { return fail(errc::io_error); }
    const result<std::uint64_t> page_size = query_page_size(host.get());
    if (!page_size) { return fail(page_size.error()); }

    ::vm_statistics64_data_t statistics {};
    ::mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    const ::kern_return_t status = ::host_statistics64(
        host.get(), HOST_VM_INFO64,
        reinterpret_cast<::host_info64_t>(&statistics), &count);
    if (status != KERN_SUCCESS) { return fail(errc::io_error); }

    const std::uint64_t pages =
        static_cast<std::uint64_t>(statistics.free_count) +
        static_cast<std::uint64_t>(statistics.inactive_count);
    if (*page_size != 0U && pages >
            (std::numeric_limits<std::uint64_t>::max)() / *page_size) {
        return fail(errc::value_too_large);
    }
    return pages * *page_size;
}

/// Reads the binary struct xsw_usage reported by the vm.swapusage sysctl.
///
/// The kernel publishes this sysctl as fixed-layout binary data, not text;
/// zero totals are valid data that mean no swap is configured.
inline result<memory_common::swap_usage> swap_status() {
    std::size_t size = 0U;
    if (::sysctlbyname("vm.swapusage", nullptr, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(::xsw_usage)) { return fail(errc::malformed_data); }
    ::xsw_usage native {};
    size = sizeof(native);
    if (::sysctlbyname("vm.swapusage", &native, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
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
