#ifndef SYSCAPE_MEMORY_HPP
#define SYSCAPE_MEMORY_HPP

/// @file
/// @brief Hosted system memory capacity and usage queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux implements every query through the kernel-documented
/// /proc/meminfo interface and POSIX sysconf values. Windows implements the
/// capacity queries through GlobalMemoryStatusEx and GetSystemInfo and
/// reports not_supported for paging space because its public APIs expose
/// only process-scoped commit accounting. macOS implements physical memory
/// through sysctl, available memory through documented Mach host statistics,
/// and swap through the binary struct xsw_usage reported by the
/// vm.swapusage sysctl. Other targets use the not-supported fallback.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/memory.hpp requires C++17 or later"
#endif

#include <cstdint>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/memory/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/memory/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/memory/macos.hpp>
#else
#include <syscape/detail/memory/generic.hpp>
#endif

namespace syscape {
namespace memory {

/// Returns the virtual-memory page size of the running system in bytes.
///
/// The page size is the allocation and protection granularity reported by the
/// operating system. It is fixed for a running system but can differ between
/// machines, architectures, and configurations of the same product.
/// @return A positive power-of-two byte count, not_supported when the platform
/// exposes no acceptable source, or a native platform error.
inline result<std::uint64_t> page_size_bytes() {
    return detail::memory_backend::page_size_bytes();
}

/// Returns the total physical memory installed and visible to the operating
/// system, in bytes.
///
/// The count describes hardware capacity as reported by the platform. It is
/// not restricted by process limits, cgroups, job objects, or virtual-machine
/// memory configuration beyond what the host reports. The value normally
/// remains unchanged while the process runs; hot-added memory can change it.
/// @return A positive byte count, not_supported when no acceptable source
/// exists, not_found when the platform source omits total capacity,
/// malformed_data for inconsistent platform data, or a native platform error.
inline result<std::uint64_t> physical_memory_bytes() {
    return detail::memory_backend::physical_memory_bytes();
}

/// Returns the operating system's estimate of memory allocatable without
/// swapping, in bytes.
///
/// This is an estimate that the platform defines itself. Linux reports its
/// MemAvailable calculation; macOS reports free and inactive pages, which
/// already contain the kernel's volatile purgeable population; Windows
/// reports currently available physical memory. The estimate excludes cached
/// data that can be reclaimed on demand only where the platform says so. The
/// value changes continuously with system load.
/// @return A byte count no greater than physical_memory_bytes(),
/// not_supported when the platform does not expose such an estimate (for
/// example kernels older than MemAvailable's introduction), malformed_data,
/// or a native platform error.
inline result<std::uint64_t> available_memory_bytes() {
    return detail::memory_backend::available_memory_bytes();
}

/// Swap or pagefile capacity at the moment of the query.
struct swap_information {
    /// Configured paging-space capacity in bytes. Zero is valid data that
    /// means the platform has no configured paging space.
    std::uint64_t total_bytes;
    /// Unused paging-space capacity in bytes.
    std::uint64_t free_bytes;
};

/// Returns configured swap or pagefile capacity and unused capacity in bytes.
///
/// Linux reports swap capacity from the kernel's documented meminfo fields,
/// and macOS reports the binary xsw_usage structure published by the
/// vm.swapusage sysctl. Windows returns not_supported because its public APIs
/// expose process-scoped commit accounting rather than system paging-space
/// capacity. Zero totals mean no paging space is configured; they are valid
/// data, not errors. Both values change continuously during normal operation.
/// @return A snapshot whose free_bytes never exceeds total_bytes,
/// not_supported when the platform exposes no acceptable source, not_found
/// when the platform source omits either field, malformed_data for
/// inconsistent platform data, or a native platform error.
inline result<swap_information> swap_status() {
    const result<detail::memory_common::swap_usage> usage =
        detail::memory_common::validate_swap_usage(
            detail::memory_backend::swap_status());
    if (!usage) { return fail(usage.error()); }
    return swap_information{usage->total_bytes, usage->free_bytes};
}

} // namespace memory
} // namespace syscape

#endif
