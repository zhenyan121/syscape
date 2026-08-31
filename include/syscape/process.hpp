#ifndef SYSCAPE_PROCESS_HPP
#define SYSCAPE_PROCESS_HPP

/// @file
/// @brief Hosted process identity, execution-context, scheduling, and
/// resource-limit queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux, Windows, macOS, and FreeBSD have native backends. The FreeBSD
/// backend reports CPU affinity as unsupported. Other targets use the generic
/// not-supported fallback.
/// @note Expected failures are returned as native error codes where available,
/// or as syscape::errc values for missing, malformed, or unsupported data.
/// @note The Windows backend requires Windows 7 or later SDK declarations.
/// A lower _WIN32_WINNT or WINVER setting is rejected with a diagnostic; when
/// either macro is absent, the internal SDK boundary supplies 0x0601 only
/// while including the required Windows headers.
/// @note The Windows memory query uses the process-memory APIs declared in
/// @<psapi.h@>,
/// which map into kernel32.dll on Windows 7 and later SDKs or require linking
/// the Psapi import library on older declarations.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/process.hpp requires C++17 or later"
#endif

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/process/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/process/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/process/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/process/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/process/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/process/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/process/netbsd.hpp>
#else
#include <syscape/detail/process/generic.hpp>
#endif

namespace syscape {
namespace process {

/// Returns the operating-system identifier of the calling process.
///
/// The identifier is unique among live processes for the current operating-
/// system instance. An identifier can be reused only after its process ends,
/// and identifiers have no meaning across independent machines or boots.
/// @return A positive process identifier or a platform error.
inline result<std::uint32_t> process_id() {
    return detail::process_backend::process_id();
}

/// Returns the operating-system identifier of the calling process's parent.
///
/// The value is a snapshot taken by the query or the underlying process
/// metadata at the moment of the call. A returned zero is valid where the
/// operating system defines it to mean that the process has no live parent;
/// it is not an error sentinel. After the parent exits, the platform may
/// report a reaper or another parent-process convention instead of the
/// original creator.
/// @return A process identifier, not_supported when the platform exposes no
/// acceptable source, not_found when no parent entry exists, malformed_data
/// for invalid platform data, or a native platform error.
inline result<std::uint32_t> parent_process_id() {
    return detail::process_backend::parent_process_id();
}

/// Returns the absolute filesystem path used to start the current program.
///
/// The path is reported verbatim from the platform and is not canonicalized:
/// symbolic links, relative components, and platform annotations are
/// preserved.
/// @return The executable path as UTF-8, not_supported when the platform
/// exposes no acceptable source, malformed_data for a relative or invalid
/// result, invalid_encoding for non-UTF-8 native text, or a native platform
/// error. A previously existing file may have been renamed, unlinked, or
/// replaced after process creation.
inline result<std::string> executable_path() {
    return detail::process_common::validate_utf8_path(
        detail::process_backend::executable_path());
}

/// Returns the argument values supplied to the current program.
///
/// The first element corresponds to the platform's argv[0] where that concept
/// exists and may therefore describe the program rather than be an argument
/// passed after it. Empty argument values are valid and are preserved. The
/// collection reflects process-start arguments except where a documented
/// platform interface explicitly exposes later modification. Platform sources
/// impose size limits on the argument data; an oversized source fails with
/// value_too_large.
/// @return Zero or more UTF-8 argument values. Most executions contain at
/// least argv[0], while an empty collection is valid where the platform
/// permits execution with no argument values. Returns not_found when the
/// platform has no acceptable command-line source, malformed_data for invalid
/// framing, invalid_encoding for non-UTF-8 text, not_supported when no such
/// interface exists, or a native platform error.
inline result<std::vector<std::string>> command_line() {
    return detail::process_common::validate_utf8_arguments(
        detail::process_backend::command_line());
}

/// Returns the absolute pathname of the calling process's working directory.
///
/// The value describes the directory used to resolve relative pathnames and
/// can change whenever the process successfully changes its working directory.
/// @return The working-directory path as UTF-8, not_supported when the
/// platform exposes no acceptable source, malformed_data for a relative or
/// invalid result, invalid_encoding for non-UTF-8 native text, or a native
/// platform error such as permission denied.
inline result<std::string> working_directory() {
    return detail::process_common::validate_utf8_path(
        detail::process_backend::working_directory());
}

/// CPU execution-time amounts consumed by the calling process.
///
/// Both durations measure only the calling process and exclude reaped child
/// processes. They never decrease during the process lifetime and grow only
/// while the operating system schedules the process. The platform limits the
/// resolution: Linux reports clock ticks (commonly ten milliseconds), Windows
/// reports hundred-nanosecond units, and macOS reports nanosecond counts
/// derived from finer scheduler samples.
struct cpu_times {
    /// Time spent executing in user mode. Never negative.
    std::chrono::nanoseconds user;
    /// Time spent executing in kernel mode on behalf of the process. Never
    /// negative.
    std::chrono::nanoseconds system;
};

/// Returns the user- and kernel-mode execution time of the calling process.
///
/// The values are a snapshot taken by the query and change continuously while
/// the process runs or its threads execute. Child processes reaped before the
/// call are never included.
/// @return A snapshot with nonnegative durations, not_supported when the
/// platform exposes no acceptable source, malformed_data for invalid platform
/// data, value_too_large when the platform amount cannot be represented as a
/// duration, or a native platform error.
inline result<cpu_times> cpu_time() {
    const result<detail::process_common::cpu_time_usage> usage =
        detail::process_backend::cpu_time();
    if (!usage) { return fail(usage.error()); }
    return cpu_times{usage->user, usage->system};
}

/// Returns the best available wall-clock instant at which the current
/// process started.
///
/// Windows and macOS report the process creation time recorded by the
/// operating system. Linux derives the instant from the kernel's boot-time
/// record plus the documented start offset in clock ticks, so suspend
/// periods and system-clock adjustments can shift it relative to real wall-
/// clock time; treat the Linux result as an estimate.
/// @return A system-clock time point no later than the moment of the query,
/// not_supported when the platform exposes no acceptable source,
/// malformed_data for inconsistent platform data, value_too_large for an
/// unrepresentable value, or a native platform error.
inline result<std::chrono::system_clock::time_point> start_time() {
    return detail::process_backend::start_time();
}

/// Resident and virtual memory extents of the calling process at the moment
/// of the query.
///
/// resident_bytes is physical memory currently occupied by the process as
/// defined by the platform's resident-set concept. virtual_bytes is the
/// platform's reported virtual-memory extent: Linux reports the total virtual
/// address-space size, macOS reports the Mach task's virtual size, and
/// Windows sums reserved and committed address-space regions. The precise
/// meaning therefore differs between platforms and the values are not
/// comparable across them; each remains meaningful on its own platform.
struct memory_usage_info {
    /// Physical memory currently occupied by the process in bytes. Zero is
    /// valid only where the platform defines it.
    std::uint64_t resident_bytes;
    /// Virtual-memory extent of the process in bytes as defined by the
    /// running platform source.
    std::uint64_t virtual_bytes;
};

/// Returns the resident and virtual memory extents of the calling process.
///
/// The values are a snapshot taken by the query and change continuously with
/// execution. They describe only the calling process, not system-wide
/// memory; use syscape::memory queries for that.
/// @return A snapshot in bytes, not_supported when the platform exposes no
/// acceptable source, malformed_data when required platform fields are missing
/// or inconsistent, value_too_large for an unrepresentable product, or a
/// native platform error.
inline result<memory_usage_info> memory_usage() {
    const result<detail::process_common::memory_usage_snapshot> usage =
        detail::process_backend::memory_usage();
    if (!usage) { return fail(usage.error()); }
    return memory_usage_info{usage->resident_bytes, usage->virtual_bytes};
}

/// Returns the number of threads currently alive in the calling process.
///
/// The count includes the calling thread and is therefore always positive on
/// success. It is a snapshot taken by the query: other threads may start or
/// exit concurrently and make the next call observe a different value. The
/// Windows implementation enumerates a system thread snapshot, which can be
/// comparatively expensive on systems with many threads.
/// @return A positive thread count, not_supported when the platform exposes
/// no acceptable source, not_found when the platform source records no
/// thread for the live calling process, malformed_data for inconsistent
/// platform data, value_too_large for an unrepresentable count, or a native
/// platform error.
inline result<std::uint32_t> thread_count() {
    return detail::process_backend::thread_count();
}

/// Returns the platform scheduling priority associated with the caller.
///
/// The value is reported in each platform's own documented scale and the
/// scales are not comparable across platforms:
/// - Linux reports the calling thread's nice value because Linux records nice
///   values per thread. The documented range is -20 (most favorable) through
///   19 (least favorable).
/// - macOS reports the calling process's nice value, through 20 on Darwin's
///   documented scale.
/// - Windows maps the documented GetPriorityClass constants onto their
///   documented base priorities (4 idle through 24 realtime).
/// A lower POSIX value means more favorable scheduling, while a higher
/// Windows base priority means more favorable scheduling.
///
/// The value reflects a snapshot taken by the query and changes when the
/// corresponding process or thread priority is changed again by any
/// authorized party.
/// @return The platform-recorded scheduling priority, not_supported when the
/// platform exposes no acceptable source, malformed_data for a value outside
/// the documented range, or a native platform error.
inline result<int> priority() {
    return detail::process_backend::priority();
}

/// Returns the logical processor indices on which the caller's platform
/// scheduling context may run.
///
/// The indices use the platform's system-wide logical processor numbering,
/// matching the counts exposed by syscape::cpu. They are returned in
/// ascending order without duplicates. The collection is a snapshot taken by
/// the query; affinity changes made by any authorized party afterwards are
/// not reflected until the next call.
///
/// Platform limitations:
/// - Linux reports the calling thread's full kernel affinity mask for all
///   processor ranges because Linux records affinity per thread.
/// - Windows reports the calling process's affinity on systems with one
///   processor group. Multiple-group systems report not_supported because
///   their group-relative indices cannot satisfy the system-wide numbering
///   contract.
/// - macOS exposes no documented public interface for this query and
///   reports not_supported.
/// @return At least one ascending unique logical processor index,
/// not_supported when the platform exposes no acceptable source or cannot
/// represent its processor numbering without ambiguity,
/// malformed_data for an empty or inconsistent mask, value_too_large for an
/// index beyond the representable range, or a native platform error.
inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return detail::process_backend::cpu_affinity();
}

/// Identifies which recorded process resource limit resource_limit()
/// reports.
///
/// Each kind names the unit used by the reported amounts. The set of kinds
/// can grow in future releases without breaking source compatibility;
/// platforms that record none of a kind report not_supported for it.
enum class resource_kind {
    /// Maximum core-file size in bytes.
    core_file_size,
    /// Maximum accumulated CPU time in seconds.
    cpu_time,
    /// Maximum size in bytes of one file the process may write.
    file_size,
    /// Maximum number of simultaneously open file descriptors.
    open_files,
    /// Maximum size in bytes of the process stack segment.
    stack_size,
    /// Maximum total virtual address-space extent in bytes.
    address_space,
};

/// One recorded bound of a process resource limit.
struct resource_limit_amount {
    /// The recorded bound in the unit named by the queried kind. Meaningful
    /// only when unlimited is false.
    std::uint64_t amount;
    /// True when the platform records no bound instead of a finite amount;
    /// amount is then unspecified and must be ignored.
    bool unlimited;
};

/// Soft and hard bounds of one process resource limit.
struct resource_limits {
    /// The currently enforced bound. Exceeding it triggers the platform's
    /// enforcement action for that kind.
    resource_limit_amount soft;
    /// The ceiling to which an unprivileged process may raise the soft bound
    /// through platform-specific means. Syscape itself provides no operation
    /// to change limits.
    resource_limit_amount hard;
};

/// Returns the recorded soft and hard bounds of one process resource limit.
///
/// The values are a snapshot taken by the query and reflect the limits at
/// that moment; they can change between calls. The soft bound never exceeds
/// the hard bound where both are finite, and an unlimited soft bound implies
/// an unlimited hard bound on the documented sources.
///
/// Linux and macOS read the POSIX getrlimit records for each kind. Windows
/// exposes no per-process equivalent through a public documented source and
/// reports not_supported for every kind.
/// @param kind The limit to query.
/// @return The recorded bounds, not_supported when the platform exposes no
/// acceptable source for the kind, invalid_argument for an unrecognized kind,
/// malformed_data for inconsistent platform records such as a soft bound
/// above the hard bound, or a native platform error.
inline result<resource_limits> resource_limit(resource_kind kind) {
    detail::process_common::limit_resource selected {};
    switch (kind) {
        case resource_kind::core_file_size:
            selected = detail::process_common::limit_resource::core_file_size;
            break;
        case resource_kind::cpu_time:
            selected = detail::process_common::limit_resource::cpu_time;
            break;
        case resource_kind::file_size:
            selected = detail::process_common::limit_resource::file_size;
            break;
        case resource_kind::open_files:
            selected = detail::process_common::limit_resource::open_files;
            break;
        case resource_kind::stack_size:
            selected = detail::process_common::limit_resource::stack_size;
            break;
        case resource_kind::address_space:
            selected = detail::process_common::limit_resource::address_space;
            break;
        default:
            return fail(errc::invalid_argument);
    }
    const result<detail::process_common::resource_limit_snapshot> snapshot =
        detail::process_backend::resource_limit(selected);
    if (!snapshot) { return fail(snapshot.error()); }
    resource_limits limits;
    limits.soft.amount = snapshot->soft.amount;
    limits.soft.unlimited = snapshot->soft.unlimited;
    limits.hard.amount = snapshot->hard.amount;
    limits.hard.unlimited = snapshot->hard.unlimited;
    return limits;
}

} // namespace process
} // namespace syscape

#endif
