#ifndef SYSCAPE_CPU_HPP
#define SYSCAPE_CPU_HPP

/// @file
/// @brief Hosted CPU identity, topology, frequency, and utilization queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux implements identity and topology through kernel-documented
/// interfaces, reads recorded frequency bounds and current clocks from the
/// cpufreq sysfs interface with a /proc/cpuinfo fallback, and folds the
/// documented /proc/stat aggregate counters into cumulative utilization
/// totals. Windows implements topology using Windows 7 or later
/// processor-group APIs, reads processor clocks through CallNtPowerInformation
/// (declared in <powerbase.h>; provided by PowrProf.lib), and folds the
/// GetSystemTimes totals into cumulative utilization on single-group
/// systems, reporting not_supported on multi-group systems where those
/// totals cover only the calling thread's processor group. macOS implements core
/// counts, reads recorded clock bounds from the hw.cpufrequency sysctl values,
/// and folds the host_processor_info scheduler ticks into cumulative
/// utilization; platforms without those facts report not_supported. All other
/// targets use the not-supported fallback.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/cpu.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/cpu/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/cpu/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/cpu/macos.hpp>
#else
#include <syscape/detail/cpu/generic.hpp>
#endif

namespace syscape {
namespace cpu {

/// Returns the distinct CPU vendor identifiers exposed by the platform.
///
/// The result preserves the platform's textual identifiers as UTF-8 and may
/// contain more than one value on a heterogeneous system. An identifier can be
/// a name such as an x86 CPUID vendor string or a numeric code exposed as text;
/// Syscape does not guess a marketing name. Values normally remain unchanged
/// during a process but can change when virtual or hot-pluggable hardware is
/// reconfigured.
/// @return One or more identifiers, not_found when the platform source contains
/// none, not_supported when no acceptable source exists, or a platform I/O,
/// malformed-data, or encoding error.
inline result<std::vector<std::string>> vendor_identifiers() {
    return detail::cpu_common::validate_utf8_labels(
        detail::cpu_backend::vendor_identifiers());
}

/// Returns the distinct CPU model labels exposed by the platform.
///
/// The result preserves the platform's labels as UTF-8 and may contain more
/// than one value on a heterogeneous system. The labels are descriptive, not
/// stable identifiers. They normally remain unchanged during a process but can
/// change when virtual or hot-pluggable hardware is reconfigured.
/// @return One or more labels, not_found when the platform source contains no
/// model label, not_supported when no acceptable source exists, or a platform
/// I/O, malformed-data, or encoding error.
inline result<std::vector<std::string>> model_names() {
    return detail::cpu_common::validate_utf8_labels(
        detail::cpu_backend::model_names());
}

/// Returns the number of logical processors currently online system-wide.
///
/// This count describes processors enabled by the operating system. It is not
/// restricted to the calling process's affinity or container CPU quota and can
/// change while the process is running.
/// @return A positive processor count or not_supported, malformed_data,
/// value_too_large, or a native platform error.
inline result<std::uint32_t> online_logical_processor_count() {
    return detail::cpu_backend::online_logical_processor_count();
}

/// Returns the number of physical CPU cores with an online logical processor.
///
/// This count is system-wide rather than restricted by process affinity and can
/// change when processors are enabled or disabled.
/// @return A positive core count or not_supported when the platform exposes no
/// physical topology, malformed_data, value_too_large, or a native error.
inline result<std::uint32_t> online_physical_core_count() {
    return detail::cpu_backend::online_physical_core_count();
}

/// Returns the number of CPU packages containing an online logical processor.
///
/// A package is a physical processor socket or equivalent package grouping
/// reported by the operating system. The count can change with CPU hot-plug.
/// @return A positive package count or not_supported when the platform exposes
/// no package topology, malformed_data, value_too_large, or a native error.
inline result<std::uint32_t> online_processor_package_count() {
    return detail::cpu_backend::online_processor_package_count();
}

/// Returns the lowest recorded operating frequency of any online logical
/// processor, in kilohertz.
///
/// This is the platform's recorded lower bound for clock selection, not a
/// measurement of instantaneous behavior. The value normally remains
/// unchanged while the process runs but follows hardware or firmware
/// reconfiguration. Linux reads the documented cpufreq cpuinfo_min_freq
/// attributes and reports not_supported when no online processor exposes
/// them, which is common in virtual machines. Windows documents no public
/// source for this bound and reports not_supported. macOS reads the
/// hw.cpufrequency_min sysctl, which Intel Macs provide and Apple silicon
/// does not.
/// @return A positive frequency or not_supported when no acceptable platform
/// source exists, malformed_data, value_too_large, or a native error.
inline result<std::uint32_t> minimum_frequency_khz() {
    return detail::cpu_backend::minimum_frequency_khz();
}

/// Returns the highest recorded operating frequency of any online logical
/// processor, in kilohertz.
///
/// This is the platform's recorded upper bound for clock selection. It is not
/// a guarantee that any processor sustains that rate and can be exceeded by
/// boost behavior on some platforms. Availability matches
/// minimum_frequency_khz(): Linux reads the cpufreq cpuinfo_max_freq
/// attributes, Windows reads the MaxMhz field of the documented processor
/// power information records on single-group systems, and macOS reads the
/// hw.cpufrequency_max sysctl. Windows multi-group systems report
/// not_supported because the documented record sizing is group-relative.
/// @return A positive frequency or not_supported when no acceptable platform
/// source exists, malformed_data, value_too_large, or a native error.
inline result<std::uint32_t> maximum_frequency_khz() {
    return detail::cpu_backend::maximum_frequency_khz();
}

/// Returns one current clock reading per online logical processor, in
/// kilohertz.
///
/// Every entry is an instantaneous sample observed during the call; entries
/// are listed in ascending online-processor order on Linux and change
/// continuously with governor decisions, load, and power state. Heterogeneous
/// systems may legitimately report different values. Linux prefers the
/// documented scaling_cur_freq attribute of every online processor and falls
/// back to the /proc/cpuinfo "cpu MHz" fields (rounded to kilohertz) when the
/// cpufreq interface is unavailable; Windows reports the CurrentMhz field of
/// the processor power information records on single-group systems and reports
/// not_supported on multi-group systems. No other backend currently has a
/// documented per-processor clock source.
/// @return One positive frequency per online logical processor,
/// not_supported when no acceptable platform source exists, malformed_data,
/// value_too_large, or a native error.
inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    return detail::cpu_backend::current_frequencies_khz();
}

/// Cumulative system-wide processor time counters at the moment of the call.
///
/// Each field normally grows from platform start until an underlying counter
/// wraps. One tick is whatever unit the queried platform uses for
/// processor-time accounting: kernel scheduler ticks on Linux and macOS, and
/// hundred-nanosecond units on Windows. Only differences between two snapshots
/// carry meaning, so callers compute utilization as delta(user) + delta(system)
/// divided by that sum plus delta(idle). A caller must discard an interval if
/// any later bucket is smaller because the snapshots crossed a counter wrap or
/// a platform accounting reset. Tick durations differ across platforms, so
/// absolute counters must never be compared across platforms.
struct usage_snapshot {
    /// Cumulative time attributed to user execution, including each
    /// platform's nice or low-priority user states.
    std::uint64_t user_ticks;
    /// Cumulative time attributed to kernel execution on behalf of the
    /// workload: system, interrupt, and softirq time on Linux, and kernel
    /// time excluding idle on Windows.
    std::uint64_t system_ticks;
    /// Cumulative time recorded as idle, including Linux iowait because that
    /// state also leaves the processor without runnable work.
    std::uint64_t idle_ticks;
};

/// Returns cumulative system-wide processor time counters.
///
/// The totals cover every processor visible to the caller, including time
/// before the process started, and are not restricted by process affinity or
/// container quotas. Platforms record further states that belong to no
/// portable bucket: Linux steal time describes execution on behalf of other
/// virtual machines and is deliberately excluded, so bucket deltas may sum to
/// less than elapsed time under hosting overhead. Queries read a consistent
/// platform snapshot and remain safe for concurrent calls.
/// @note macOS supplies per-processor natural_t counters. Their system-wide
/// sums can decrease when an underlying counter wraps; the query deliberately
/// keeps no mutable process-wide state, so callers must discard such an
/// interval and take another pair of snapshots.
/// @note On Windows the totals fold the documented GetSystemTimes counters,
/// which summarize only the calling thread's primary processor group on
/// systems with more than one processor group. Such multi-group systems
/// report not_supported rather than silently partial coverage.
/// @return Cumulative counters, not_found when the platform source contains
/// no aggregate record, not_supported when no acceptable source exists or
/// the Windows system has more than one processor group, malformed_data,
/// value_too_large, or a native error.
inline result<usage_snapshot> cumulative_processor_usage() {
    const result<detail::cpu_common::usage_information> value =
        detail::cpu_backend::cumulative_processor_usage();
    if (!value) { return fail(value.error()); }
    return usage_snapshot{value->user_ticks, value->system_ticks,
                          value->idle_ticks};
}

} // namespace cpu
} // namespace syscape

#endif
