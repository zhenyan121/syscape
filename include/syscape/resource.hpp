#ifndef SYSCAPE_RESOURCE_HPP
#define SYSCAPE_RESOURCE_HPP

/// @file
/// @brief Hosted system-wide resource and capacity queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux implements most queries through kernel-documented /proc
/// interfaces and reports not_supported for the open-handle total because
/// the platform documents no source for that population. Windows reports
/// process and thread totals plus the
/// system-wide open-handle total through GetPerformanceInfo (declared in
/// <psapi.h>; provided by Kernel32.lib on Windows 7 or later SDKs, otherwise
/// Psapi.lib) and reports not_supported for its load-average,
/// scheduling-entity, open-file-count, and descriptor-limit queries because
/// the platform documents no public source matching those contracts.
/// macOS reads the load average through getloadavg, counts processes by
/// enumerating the documented KERN_PROC_ALL table, and reads the thread
/// count and open-file count from the long-stable XNU sysctl values
/// kern.num_threads and kern.num_files; those sysctls are not described in
/// Apple's formal documentation set and are used because no stronger
/// documented source exists on that platform. FreeBSD implements load average,
/// process count, open-file count, and the system file limit; scheduler entity,
/// thread, and handle totals report not_supported. All other targets use the
/// not-supported fallback.
/// @note Every count is an instantaneous snapshot observed during the call;
/// values change continuously with system activity and must never be
/// assumed stable after a query returns.
/// @note On Linux, process and thread visibility follows the proc
/// filesystem's mount configuration (such as hidepid options), so counts
/// describe what the calling user can observe rather than an absolute
/// system total.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/resource.hpp requires C++17 or later"
#endif

#include <cstdint>

#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/resource/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/resource/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/resource/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/resource/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/resource/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/resource/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/resource/dragonfly.hpp>
#else
#include <syscape/detail/resource/generic.hpp>
#endif

namespace syscape {
namespace resource {

/// System load averages at the moment of the query.
///
/// Load averages are dimensionless exponentially damped measures of demand
/// for the system's processors. Platforms define their own composition:
/// Linux includes runnable and uninterruptible tasks, while macOS uses the
/// BSD equivalent. The values are therefore comparable across time on one
/// platform but are not normalized across platforms.
struct load_snapshot {
    /// Exponentially damped load average over the last one minute.
    double one_minute;
    /// Exponentially damped load average over the last five minutes.
    double five_minute;
    /// Exponentially damped load average over the last fifteen minutes.
    double fifteen_minute;
};

/// Scheduler entity totals at the moment of the query.
struct scheduling_snapshot {
    /// Number of scheduling entities currently runnable on a processor.
    std::uint64_t runnable_entities;
    /// Total number of schedulable entities that currently exist. This is
    /// a scheduler population figure, not a process or thread census, and
    /// platforms may define membership differently from those queries.
    std::uint64_t total_entities;
};

/// Returns the system's one-, five-, and fifteen-minute load averages.
///
/// The samples are dimensionless and nonnegative; zero samples are valid
/// data for an idle system. The values change continuously with system
/// load. Linux reads them from the kernel-documented /proc/loadavg file,
/// and macOS uses the documented getloadavg interface. Windows exposes no
/// documented load-average source and reports not_supported rather than
/// substituting a utilization measurement.
/// @return A snapshot of finite nonnegative samples, not_supported when the
/// platform exposes no acceptable source, malformed_data for platform data
/// outside the documented format, or a native platform error.
inline result<load_snapshot> load_average() {
    const result<detail::resource_common::load_samples> samples =
        detail::resource_common::validate_load_samples(
            detail::resource_backend::load_average());
    if (!samples) { return fail(samples.error()); }
    return load_snapshot{samples->one_minute, samples->five_minute,
                         samples->fifteen_minute};
}

/// Returns the number of runnable scheduling entities and the total number
/// of schedulable entities observed during the call.
///
/// Linux reports both counts from the fourth field of the kernel-documented
/// /proc/loadavg file, whose running/total pair describes the scheduler's
/// entity populations. Other platforms expose no documented equivalent and
/// report not_supported. Both counts change continuously with system
/// activity.
/// @return A snapshot whose runnable count never exceeds its total,
/// not_supported when the platform exposes no such source, malformed_data
/// for platform data outside the documented format, or a native platform
/// error.
inline result<scheduling_snapshot> scheduler_entities() {
    const result<detail::resource_common::entity_counts> entities =
        detail::resource_common::validate_entity_counts(
            detail::resource_backend::scheduler_entities());
    if (!entities) { return fail(entities.error()); }
    return scheduling_snapshot{entities->runnable, entities->schedulable};
}

/// Returns the number of processes existing on the system during the call.
///
/// On Linux the count enumerates the numeric entries of the kernel-documented
/// /proc directory, so the cost grows with the number of running processes
/// and the result covers only entries visible to the calling user under
/// restrictions such as hidepid. Windows reports ProcessCount from the
/// documented GetPerformanceInfo call in a single system snapshot. macOS
/// counts the entries of the documented KERN_PROC_ALL table. The calling
/// process always exists, so zero cannot be a valid result.
/// @return A positive count, not_supported when the platform exposes no
/// acceptable source, value_too_large when the total exceeds the portable
/// representation, malformed_data for inconsistent platform data, or a
/// native platform error.
inline result<std::uint64_t> process_count() {
    return detail::resource_common::validate_positive_count(
        detail::resource_backend::process_count());
}

/// Returns the number of threads existing on the system during the call.
///
/// On Linux the count sums the num_threads field across every readable
/// kernel-documented /proc/[pid]/stat record, so the cost grows with the
/// number of running processes; only records whose process exits between
/// listing and reading (a vanished directory entry or an empty read) are
/// skipped as expected races, while permission, input, and format failures
/// propagate as errors, so a returned total always covers every record the
/// caller could read. Visibility follows the proc mount's hidepid
/// configuration. Windows reports ThreadCount from the documented
/// GetPerformanceInfo call. macOS reads the XNU kern.num_threads sysctl,
/// which is long-stable but absent from Apple's formal documentation set.
/// The calling thread always exists, so zero cannot be a valid result.
/// @return A positive count, not_supported when the platform exposes no
/// acceptable source, value_too_large, malformed_data, or a native
/// platform error. Each query takes an independent snapshot, so results of
/// separate calls observe different instants and carry no ordering
/// guarantee relative to each other.
inline result<std::uint64_t> thread_count() {
    return detail::resource_common::validate_positive_count(
        detail::resource_backend::thread_count());
}

/// Returns the operating system's count of open files during the call.
///
/// Each platform counts its own notion of an open file: Linux reports
/// allocated kernel file handles from the first value of the
/// kernel-documented /proc/sys/fs/file-nr file, and macOS reports the XNU
/// kern.num_files sysctl under the same documentation limitation as
/// thread_count(). Windows exposes no documented source for a file-oriented
/// total because its performance information counts every kernel-object
/// handle kind, so it reports not_supported rather than presenting
/// unrelated data. Zero is valid data wherever a platform can genuinely
/// reach it; the counts are not normalized into one cross-platform meaning.
/// @return A count, not_supported when the platform exposes no acceptable
/// source, malformed_data, or a native platform error.
inline result<std::uint64_t> open_file_count() {
    return detail::resource_backend::open_file_count();
}

/// Returns the operating system's total of open handles to kernel objects
/// during the call.
///
/// Handles include every object kind the platform defines, such as files,
/// processes, threads, events, and synchronization primitives; this query
/// reports that all-handles population verbatim instead of narrowing it,
/// which distinguishes it from open_file_count(). Windows reports
/// HandleCount from the documented GetPerformanceInfo call. Linux and macOS
/// expose no documented equivalent total and report not_supported. Zero is
/// valid data wherever a platform can genuinely reach it.
/// @return A count, not_supported when the platform exposes no acceptable
/// source, malformed_data, or a native platform error.
inline result<std::uint64_t> open_handle_count() {
    return detail::resource_backend::open_handle_count();
}

/// Returns the system-wide limit on open files, descriptors, or handles
/// that the platform enforces.
///
/// Linux reports the maximum allocated-file-handle count from the third
/// value of the kernel-documented /proc/sys/fs/file-nr file, and macOS
/// reports the BSD-documented kern.maxfiles sysctl. Windows documents no
/// configurable system-wide handle limit and returns not_supported rather
/// than presenting an implementation bound as policy. The limit normally
/// remains fixed while the system runs; administrators can reconfigure it.
/// @return A positive limit, not_supported when the platform exposes no
/// documented limit, malformed_data for inconsistent platform data, or a
/// native platform error.
inline result<std::uint64_t> file_descriptor_limit() {
    return detail::resource_common::validate_positive_count(
        detail::resource_backend::file_descriptor_limit());
}

} // namespace resource
} // namespace syscape

#endif
