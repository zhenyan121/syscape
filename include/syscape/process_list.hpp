#ifndef SYSCAPE_PROCESS_LIST_HPP
#define SYSCAPE_PROCESS_LIST_HPP

/// @file
/// @brief Hosted process enumeration and observable metadata queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms, Android, and OpenHarmony).
/// @note This module exposes:
/// - Enumeration of all observable processes on the system (processes()).
/// - Total count of observable live processes (process_count()).
/// - Lookup of process metadata by PID (find_process(pid)).
/// - Lookup of processes by command/executable name
/// (find_processes_by_name(name)).
/// - Process lifecycle execution state (running, sleeping, stopped, zombie).
/// - Essential process attributes (PID, PPID, UID, GID, username, comm name,
///   executable path, command-line arguments, working directory, start time,
///   user and system CPU times, resident and virtual memory, thread count,
///   priority).
/// @note Linux queries procfs (/proc/[pid]/...).
/// @note Windows queries Toolhelp32 snapshots and Process APIs (tlhelp32.h,
/// psapi.h).
/// @note macOS queries sysctl (KERN_PROC_ALL) and libproc APIs.
/// @note Apple mobile platforms (iOS, iPadOS, tvOS, watchOS, visionOS, and
/// Mac Catalyst) report permission_denied for system-wide process enumeration
/// under application sandbox rules; find_process(0) reports not_found.
/// @note Android queries procfs (/proc/[pid]/...).
/// @note Solaris queries procfs (/proc/[pid]/psinfo, /proc/[pid]/path).
/// @note Processes and their metadata change continuously. Queries do not cache
/// results. Unprivileged callers gracefully receive partial observable metadata
/// for restricted processes rather than failing the enumeration.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/process_list.hpp requires C++17 or later"
#endif

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace syscape {
namespace process_list {

/// Operational and scheduling lifecycle state of a process.
enum class process_state : std::uint8_t {
    /// State could not be determined or is unclassified.
    unknown,
    /// Actively executing or runnable on a CPU.
    running,
    /// Sleeping (interruptible or uninterruptible wait for I/O, timer, or event).
    sleeping,
    /// Stopped or traced by a signal or debugger.
    stopped,
    /// Terminated but not yet reaped by its parent.
    zombie
};

/// Observable metadata describing a single system process.
struct process_entry {
    /// Operating-system process identifier (PID).
    std::uint32_t pid = 0;

    /// Parent process identifier (PPID), if reported by the platform.
    std::optional<std::uint32_t> ppid;

    /// Real user ID of the process owner on POSIX systems.
    std::optional<std::uint32_t> uid;

    /// Real group ID of the process owner on POSIX systems.
    std::optional<std::uint32_t> gid;

    /// Human-readable username of the process owner, or no value when it
    /// cannot be resolved.
    std::optional<std::string> user_name;

    /// Short process or command name, or no value when it is not observable.
    std::optional<std::string> name;

    /// Absolute path to the executable binary, or no value when inaccessible.
    std::optional<std::string> executable_path;

    /// Command-line argument vector, or no value when inaccessible. An empty
    /// vector is a successfully observed command line with no arguments.
    std::optional<std::vector<std::string>> command_line;

    /// Absolute working directory path, or no value when inaccessible.
    std::optional<std::string> working_directory;

    /// Process lifecycle execution state.
    process_state state = process_state::unknown;

    /// Cumulative user-mode CPU execution time, or no value when unavailable.
    std::optional<std::chrono::nanoseconds> user_cpu_time;

    /// Cumulative kernel/system-mode CPU execution time, or no value when
    /// unavailable.
    std::optional<std::chrono::nanoseconds> kernel_cpu_time;

    /// Best available process creation time point.
    std::optional<std::chrono::system_clock::time_point> start_time;

    /// Resident memory size in bytes (RSS on POSIX, Working Set on Windows),
    /// or no value when unavailable.
    std::optional<std::uint64_t> resident_memory_bytes;

    /// Virtual memory size in bytes, or no value when unavailable.
    std::optional<std::uint64_t> virtual_memory_bytes;

    /// Number of active threads in the process, or no value when unavailable.
    std::optional<std::uint32_t> thread_count;

    /// Platform-specific scheduling priority or nice level, if exposed.
    std::optional<int> priority;
};

} // namespace process_list
} // namespace syscape

#include <syscape/detail/process_list/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__) && !defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/process_list/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/process_list/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/process_list/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/process_list/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/process_list/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/process_list/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/process_list/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/process_list/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/process_list/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/process_list/openharmony.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/process_list/solaris.hpp>
#else
#include <syscape/detail/process_list/generic.hpp>
#endif

namespace syscape {
namespace process_list {

/// Returns all observable processes currently active on the system.
///
/// The resulting collection is sorted in natural ascending order by process ID
/// (PID). A successful empty collection means the current caller can observe no
/// processes in its platform or sandbox scope.
/// @return A vector of process_entry structures, or a platform error.
inline result<std::vector<process_entry>> processes() {
    return detail::process_list_backend::processes();
}

/// Returns the total number of observable live processes on the system.
///
/// The count uses the same observability rules as processes(); zero is valid
/// when no processes are observable. Separate calls can differ because
/// processes may start or exit between snapshots.
/// @return A process count, or a platform error.
inline result<std::uint32_t> process_count() {
    return detail::process_list_backend::process_count();
}

/// Queries metadata for a specific process ID.
///
/// @param pid The positive operating-system process ID to query. Zero is not
/// a queryable process identifier.
/// @return The matching process_entry, not_found if the process does not exist
/// or has exited, or a platform error.
inline result<process_entry> find_process(std::uint32_t pid) {
    return detail::process_list_backend::find_process(pid);
}

/// Queries all observable processes matching a given executable or command name.
///
/// Matches against the process short name or executable basename.
/// @param name The process name to search for (case-insensitive on Windows).
/// @return A vector of matching process_entry structures, or a platform error.
inline result<std::vector<process_entry>> find_processes_by_name(
    std::string_view name) {
    return detail::process_list_backend::find_processes_by_name(name);
}

} // namespace process_list
} // namespace syscape

#endif // SYSCAPE_PROCESS_LIST_HPP
