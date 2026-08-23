#ifndef SYSCAPE_DETAIL_RESOURCE_WINDOWS_HPP
#define SYSCAPE_DETAIL_RESOURCE_WINDOWS_HPP

#include <cstdint>
#include <system_error>
#include <windows.h>

#include <psapi.h>

#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

inline std::error_code last_error() {
    return std::error_code(static_cast<int>(::GetLastError()),
                           std::system_category());
}

/// System-wide totals taken from one GetPerformanceInfo call.
struct performance_snapshot {
    /// Total open handles system-wide. Zero is valid data.
    std::uint64_t handle_count = 0U;
    /// Number of processes currently existing.
    std::uint64_t process_count = 0U;
    /// Number of threads currently existing.
    std::uint64_t thread_count = 0U;
};

/// Validates one performance-information snapshot into portable totals.
///
/// The calling process and its threads exist while the query runs, so a
/// zero process or thread count cannot describe a live system and is
/// malformed platform data. A zero handle count is accepted as data.
inline result<performance_snapshot> interpret_performance_information(
    std::uint64_t handle_count,
    std::uint64_t processes,
    std::uint64_t threads) {
    if (processes == 0U || threads == 0U) {
        return fail(errc::malformed_data);
    }
    performance_snapshot snapshot;
    snapshot.handle_count = handle_count;
    snapshot.process_count = processes;
    snapshot.thread_count = threads;
    return snapshot;
}

inline result<performance_snapshot> read_performance_information() {
    ::PERFORMANCE_INFORMATION information {};
    information.cb = static_cast<::DWORD>(sizeof(information));
    if (::GetPerformanceInfo(&information,
                             static_cast<::DWORD>(sizeof(information))) ==
        FALSE) {
        return fail(last_error());
    }
    return interpret_performance_information(
        static_cast<std::uint64_t>(information.HandleCount),
        static_cast<std::uint64_t>(information.ProcessCount),
        static_cast<std::uint64_t>(information.ThreadCount));
}

/// Returns not_supported because Windows exposes no load-average value
/// through a documented public source.
///
/// The processor-time counters exposed by Windows measure utilization over
/// an interval, which is a different quantity from the exponentially damped
/// demand measures that other platforms report as load averages.
inline result<resource_common::load_samples> load_average() {
    return fail(errc::not_supported);
}

/// Returns not_supported because Windows exposes no scheduling-entity
/// counts through a documented public source.
inline result<resource_common::entity_counts> scheduler_entities() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> process_count() {
    const result<performance_snapshot> snapshot =
        read_performance_information();
    if (!snapshot) { return fail(snapshot.error()); }
    return snapshot->process_count;
}

inline result<std::uint64_t> thread_count() {
    const result<performance_snapshot> snapshot =
        read_performance_information();
    if (!snapshot) { return fail(snapshot.error()); }
    return snapshot->thread_count;
}

/// Returns not_supported because Windows exposes no open-file total through
/// a documented public source.
///
/// PERFORMANCE_INFORMATION::HandleCount counts every open handle to every
/// kernel object kind, including processes, threads, events, and
/// synchronization primitives. That population is a different quantity from
/// the file-oriented totals other platforms report for this query, so
/// reporting it here would present unrelated data as an open-file count.
inline result<std::uint64_t> open_file_count() {
    return fail(errc::not_supported);
}

/// Returns the system-wide total of open handles from the documented
/// GetPerformanceInfo call.
///
/// Handles include every kernel-object kind the platform defines, not only
/// files; this query reports that platform notion verbatim instead of
/// narrowing it.
inline result<std::uint64_t> open_handle_count() {
    const result<performance_snapshot> snapshot =
        read_performance_information();
    if (!snapshot) { return fail(snapshot.error()); }
    return snapshot->handle_count;
}

/// Returns not_supported because Windows documents no system-wide limit on
/// the total number of handles.
///
/// The 16-million theoretical handle ceiling is an implementation bound of
/// the kernel's handle table, not a configurable documented platform limit,
/// and reporting it would fabricate a policy value.
inline result<std::uint64_t> file_descriptor_limit() {
    return fail(errc::not_supported);
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
