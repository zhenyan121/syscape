#ifndef SYSCAPE_MEMORY_HPP
#define SYSCAPE_MEMORY_HPP

/// @file
/// @brief Hosted system memory capacity, commit accounting, huge pages,
/// utilization, and pressure-stall usage queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux implements every query through the kernel-documented
/// /proc/meminfo interface, POSIX sysconf values, and the kernel-documented
/// /proc/pressure/memory records. Windows implements the capacity, commit,
/// load, and huge-page-size queries through GlobalMemoryStatusEx,
/// GetPerformanceInfo, GetSystemInfo, and GetLargePageMinimum, and reports
/// not_supported for paging space because its public APIs do not separately
/// expose paging-space capacity. macOS implements physical memory through
/// sysctl, available memory and the load estimate through documented Mach
/// host statistics, and swap through the binary struct xsw_usage reported by
/// the vm.swapusage sysctl; Darwin exposes no public commit, huge-page, or
/// pressure source. FreeBSD implements page size, physical and available
/// memory, swap, and memory load through sysconf and documented sysctl values;
/// commit, huge-page, and pressure queries report not_supported. Android
/// implements page size, physical and available memory, swap, and memory load
/// through sysconf and /proc/meminfo. Other targets use the not-supported
/// fallback.
/// @note The Windows commit query uses GetPerformanceInfo declared in
/// <psapi.h>, which maps to Kernel32.lib on Windows 7 or later SDKs and may
/// require Psapi.lib with older declarations.

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
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/memory/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/memory/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/memory/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/memory/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/memory/android.hpp>
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
/// reports currently available physical memory. NetBSD uses the expanded UVM
/// snapshot's free and inactive page populations when available; its legacy
/// UVM snapshot exposes only free pages. The estimate excludes cached data
/// that can be reclaimed on demand only where the platform says so. The value
/// changes continuously with system load.
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

/// Virtual-memory commit accounting at the moment of the query.
struct commit_information {
    /// Currently committed virtual memory in bytes.
    std::uint64_t committed_bytes;
    /// The effective commit limit in bytes. The scope of this limit is
    /// platform-defined: Linux reports the kernel's CommitLimit derived from
    /// its overcommit configuration, while Windows reports the system-wide
    /// CommitLimit from GetPerformanceInfo. The committed amount can
    /// legitimately exceed the limit on platforms with heuristic overcommit.
    std::uint64_t commit_limit_bytes;
};

/// Returns the platform's virtual-memory commit charge and limit in bytes.
///
/// Linux reads the kernel's documented Committed_AS and CommitLimit meminfo
/// fields; Windows reads the system-wide CommitTotal, CommitLimit, and page
/// size from GetPerformanceInfo and converts the page counts to bytes;
/// platforms without public commit accounting report not_supported. Both
/// values change continuously during normal operation. There is deliberately
/// no ordering guarantee between the two fields.
/// @return A snapshot of current commit accounting, not_supported when the
/// platform exposes no acceptable source, not_found when the platform source
/// omits either field, malformed_data for inconsistent platform data, or a
/// native platform error.
inline result<commit_information> commit_status() {
    const result<detail::memory_common::commit_usage> usage =
        detail::memory_backend::commit_status();
    if (!usage) { return fail(usage.error()); }
    return commit_information{usage->committed_bytes,
                              usage->commit_limit_bytes};
}

/// Returns the default huge-page size in bytes.
///
/// Linux reports the Hugepagesize meminfo value; Windows reports the
/// documented GetLargePageMinimum value. Gigantic page sizes beyond the
/// platform's default (for example 1 GiB pages on x86-64 Linux) are outside
/// this contract. The value is fixed while the system runs but reflects the
/// boot-time configuration. Zero counts remain valid data elsewhere in this
/// module, but a size query cannot honestly return zero, so an empty pool
/// still reports the configured page size where one exists.
/// @return A positive power-of-two byte count, not_supported when the
/// platform exposes no acceptable source, not_found when the platform source
/// omits the size, malformed_data, or a native platform error.
inline result<std::uint64_t> huge_page_size_bytes() {
    return detail::memory_common::validate_huge_page_size(
        detail::memory_backend::huge_page_size_bytes());
}

/// Huge-page pool occupancy at the moment of the query.
struct huge_page_pool_information {
    /// Configured pool size in default-size huge pages, including any
    /// dynamically grown surplus population the platform records as part
    /// of the pool. Zero is valid data that means no pool is configured.
    std::uint64_t total_count;
    /// Unallocated pool pages. Pages reserved by future allocations remain
    /// part of this count where the platform records them that way, so a
    /// positive free count does not guarantee a successful allocation.
    std::uint64_t free_count;
};

/// Returns the configured huge-page pool occupancy in pages.
///
/// Linux reports the HugePages_Total and HugePages_Free meminfo values;
/// platforms without public pool accounting report not_supported. The counts
/// change with administrative reconfiguration and allocations. Reserved and
/// surplus refinements beyond the two recorded populations are outside this
/// contract.
/// @return A snapshot whose free_count never exceeds total_count,
/// not_supported when the platform exposes no acceptable source, not_found
/// when the platform source omits either field, malformed_data for
/// contradictory platform data, or a native platform error.
inline result<huge_page_pool_information> huge_page_pool_status() {
    const result<detail::memory_common::huge_page_pool_usage> pool =
        detail::memory_common::validate_huge_page_pool(
            detail::memory_backend::huge_page_pool_status());
    if (!pool) { return fail(pool.error()); }
    return huge_page_pool_information{pool->total_count, pool->free_count};
}

/// Returns the operating system's approximate physical-memory utilization as
/// a whole percentage from 0 through 100.
///
/// Each platform reports its own recorded notion of memory load and the
/// definitions are deliberately not normalized: Windows reports the
/// documented dwMemoryLoad percentage verbatim; Linux estimates utilization
/// as the share of physical memory outside the MemAvailable estimate; macOS
/// computes the share outside the free and inactive populations that back
/// available_memory_bytes(). The estimate changes continuously with system
/// load and is not comparable across platforms at fine granularity.
/// @return A percentage between 0 and 100, not_supported when the platform
/// exposes no acceptable source or lacks the underlying availability
/// estimate, not_found when the platform source omits total memory,
/// malformed_data for inconsistent platform data, or a native platform error.
inline result<std::uint32_t> memory_load_percent() {
    return detail::memory_backend::memory_load_percent();
}

/// One pressure-stall window with unit-carrying fields.
struct pressure_sample {
    /// Fraction of wall-clock time with stalls over the last ten seconds, in
    /// micro-percent (percent multiplied by exactly one million), so the
    /// platform's two-fractional-digit rendering converts losslessly.
    std::uint64_t average10_micro_percent;
    /// Fraction of wall-clock time with stalls over the last sixty seconds,
    /// in micro-percent.
    std::uint64_t average60_micro_percent;
    /// Fraction of wall-clock time with stalls over the last three hundred
    /// seconds, in micro-percent.
    std::uint64_t average300_micro_percent;
    /// Cumulative stall duration since boot in microseconds.
    std::uint64_t total_microseconds;
};

/// Pressure-stall snapshot for tasks stalled on memory.
struct memory_pressure_status {
    /// Stalls affecting at least one runnable task.
    pressure_sample some;
    /// Whether the platform exposed a full-stall record. Linux always sets
    /// this true because its documented memory-pressure format requires the
    /// record; the field preserves room for other platform contracts.
    bool has_full;
    /// Stalls affecting every concurrently runnable task simultaneously;
    /// meaningful only when has_full is true.
    pressure_sample full;
};

/// Returns how much wall-clock time tasks spend stalled on memory.
///
/// Linux parses the kernel-documented /proc/pressure/memory records, which
/// exist only when the kernel was built with pressure-stall information and
/// it has not been administratively disabled; other platforms expose no
/// equivalent public source and report not_supported. All averages describe
/// fractions of elapsed wall-clock time and every value is an instantaneous
/// snapshot of continuously changing counters.
/// @return A snapshot with the platform's documented pressure records,
/// not_supported when the platform exposes no acceptable source, malformed_data
/// for unparsable or contradictory records, or a native platform error.
inline result<memory_pressure_status> memory_pressure() {
    const result<detail::memory_common::pressure_status> status =
        detail::memory_backend::memory_pressure();
    if (!status) { return fail(status.error()); }
    memory_pressure_status output;
    output.some.average10_micro_percent = status->some.average10_micro_percent;
    output.some.average60_micro_percent = status->some.average60_micro_percent;
    output.some.average300_micro_percent =
        status->some.average300_micro_percent;
    output.some.total_microseconds = status->some.total_microseconds;
    output.has_full = status->has_full;
    output.full.average10_micro_percent = status->full.average10_micro_percent;
    output.full.average60_micro_percent = status->full.average60_micro_percent;
    output.full.average300_micro_percent =
        status->full.average300_micro_percent;
    output.full.total_microseconds = status->full.total_microseconds;
    return output;
}

} // namespace memory
} // namespace syscape
#endif
