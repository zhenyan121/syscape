#ifndef SYSCAPE_CPU_HPP
#define SYSCAPE_CPU_HPP

/// @file
/// @brief Hosted CPU identity, topology, frequency, utilization, cache, and
/// instruction-set queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms and Android).
/// @note Linux implements identity through kernel-documented interfaces,
/// topology and cache instances through the documented testing sysfs ABI
/// interface under /sys/devices/system/cpu, recorded frequency bounds and
/// current clocks from the cpufreq sysfs interface with a /proc/cpuinfo
/// fallback, instruction-set features from /proc/cpuinfo, and folds the
/// documented /proc/stat aggregate counters into cumulative utilization
/// totals. Windows implements topology and caches using Windows 7 or later
/// processor-group APIs, reads processor clocks through CallNtPowerInformation
/// (declared in <powerbase.h>; provided by PowrProf.lib), enumerates the
/// documented processor-feature probes of IsProcessorFeaturePresent, and
/// folds the GetSystemTimes totals into cumulative utilization on
/// single-group systems, reporting not_supported on multi-group systems
/// where those totals cover only the calling thread's processor group.
/// macOS implements core counts, reads recorded clock bounds from the
/// hw.cpufrequency sysctl values, collects the documented feature renderings,
/// and folds the host_processor_info scheduler ticks into cumulative
/// utilization. Darwin's documented cache sysctls do not identify distinct
/// sharing sets, so the cache-instance query reports not_supported. Apple
/// mobile platforms (iOS, iPadOS, tvOS, watchOS, visionOS, and Mac Catalyst)
/// implement core counts, model names (hw.model), recorded frequency bounds
/// (hw.cpufrequency_min/max), and cumulative processor usage using Mach
/// host_processor_info(); cache, vendor, and instruction-set queries report
/// not_supported. Platforms without the other facts also report not_supported.
/// FreeBSD implements model, topology counts, frequency, and cumulative usage
/// queries through documented sysctl values; vendor, cache, and instruction-set
/// queries report not_supported. Android implements core count, SOC vendor and
/// model, frequency bounds, and cumulative utilization through sysconf, system
/// properties, /sys/devices/system/cpu, and /proc/stat. Solaris implements
/// core counts, vendor identifiers, model names, and cumulative utilization
/// through sysconf and kstat, and instruction-set features through getisax;
/// frequency queries and cache descriptors report not_supported.
/// Applications using this header on Solaris link -lkstat. All other targets
/// use the not-supported fallback.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/cpu.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace syscape {
namespace cpu {

/// Kind of information stored by one processor cache instance.
enum class cache_kind : std::uint8_t {
    /// The cache stores data only.
    data,
    /// The cache stores instructions only.
    instruction,
    /// The cache stores both data and instructions.
    unified,
    /// The cache stores decoded operations rather than addressable memory.
    trace,
    /// The platform records the instance without a stored-information kind.
    unknown
};

} // namespace cpu
} // namespace syscape

#include <syscape/detail/cpu/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/cpu/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/cpu/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/cpu/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/cpu/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/cpu/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/cpu/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/cpu/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/cpu/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/cpu/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/cpu/solaris.hpp>
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

/// One distinct processor cache instance observed by the platform.
///
/// Two instances are distinct when different sets of logical processors
/// share them; a sixteen-core system therefore lists sixteen level-one
/// data caches even when every entry carries equal attributes. The
/// geometry fields describe the recorded platform values verbatim: a
/// zero in associativity_ways, sets_count, or
/// shared_logical_processor_count records that the platform exposes no
/// such value for this instance, because none of those quantities can be
/// zero on real hardware.
struct cache_information {
    /// Cache level counted from one for the level nearest the processor.
    std::uint32_t level;
    /// Recorded kind of stored information. The unknown kind records that
    /// the platform reports the instance without a stored-information
    /// type rather than forcing an invented classification.
    cache_kind kind;
    /// Size of one shared instance in bytes as recorded by the platform;
    /// always positive.
    std::uint64_t instance_size_bytes;
    /// Coherency (line) size in bytes; always positive.
    std::uint32_t line_size_bytes;
    /// Associativity expressed in ways, or zero when the platform reports
    /// no value.
    std::uint32_t associativity_ways;
    /// Number of sets, or zero when the platform reports no value. A fully
    /// associative cache is exactly one set holding every line, so a
    /// source that records full associativity converts it to one set with
    /// size divided by line size ways.
    std::uint32_t sets_count;
    /// Online logical processors that share one such instance, or zero
    /// when the platform reports no sharing count.
    std::uint32_t shared_logical_processor_count;
};

/// Returns one entry per distinct cache instance of the online processors,
/// ordered by nondecreasing level and then by kind.
///
/// The snapshot reflects the online population observed during the call;
/// hot-plug and virtualization reconfiguration become visible only to
/// later calls. Linux reads the kernel's documented testing sysfs cache ABI
/// interface under /sys/devices/system/cpu/cpuN/cache/indexI/ and reports
/// not_supported when no online processor exposes any cache directory,
/// which happens on several virtual-machine configurations. Windows reads
/// the documented GetLogicalProcessorInformationEx cache relationship on
/// single-group systems and reports not_supported on multi-group systems.
/// Darwin's documented cache sysctls do not map their per-level geometry to
/// distinct sharing sets, so macOS cannot satisfy the per-instance contract
/// and reports not_supported. Other targets report not_supported.
/// @return One or more entries, not_supported when the platform exposes no
/// acceptable source, malformed_data, value_too_large, temporarily
/// unavailable data during enumeration, or a native platform error.
inline result<std::vector<cache_information>> cache_descriptors() {
    const result<std::vector<detail::cpu_common::cache_entry>> entries =
        detail::cpu_common::validate_cache_entries(
            detail::cpu_backend::cache_descriptors());
    if (!entries) { return fail(entries.error()); }
    std::vector<cache_information> output;
    output.reserve(entries->size());
    for (const detail::cpu_common::cache_entry& entry : *entries) {
        cache_information converted;
        converted.level = entry.level;
        converted.kind = entry.kind;
        converted.instance_size_bytes = entry.instance_size_bytes;
        converted.line_size_bytes = entry.line_size_bytes;
        converted.associativity_ways = entry.associativity_ways;
        converted.sets_count = entry.sets_count;
        converted.shared_logical_processor_count =
            entry.shared_logical_processor_count;
        output.push_back(std::move(converted));
    }
    return output;
}

/// Returns the instruction-set feature identifiers the platform reports
/// for its processors.
///
/// Every identifier is the platform's own vocabulary rendered verbatim:
/// Linux collects the whitespace-separated tokens of /proc/cpuinfo's
/// documented per-architecture feature fields (flags on x86 families and
/// Features on arm and arm64), Windows renders the documented
/// IsProcessorFeaturePresent enumeration with lowercase labels derived
/// from each PF_ constant name, and macOS collects the machdep.cpu
/// feature renderings where present plus the documented hw.optional
/// boolean keys whose suffixes become the identifiers. Identifiers are
/// therefore comparable within one platform but not across platforms:
/// Syscape does not normalize genuinely different processor vocabularies
/// into invented names. An empty result is valid data meaning that the
/// platform answered but no reported feature was present. Linux unions
/// every processor block so heterogeneous populations merge into one set;
/// architectures without a recognized rendering report not_found.
/// @return Zero or more unique identifiers in first-seen order, not_found
/// when the platform source contains no recognized feature vocabulary,
/// not_supported when no acceptable source exists, malformed_data,
/// invalid_encoding, or a native platform error.
inline result<std::vector<std::string>> instruction_set_features() {
    const result<std::vector<std::string>> features =
        detail::cpu_backend::instruction_set_features();
    if (!features) { return fail(features.error()); }
    for (const std::string& identifier : *features) {
        if (identifier.empty() ||
            !detail::is_valid_utf8(identifier)) {
            return fail(errc::invalid_encoding);
        }
    }
    return features;
}

} // namespace cpu
} // namespace syscape

#endif
