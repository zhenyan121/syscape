#ifndef SYSCAPE_IPC_HPP
#define SYSCAPE_IPC_HPP

/// @file
/// @brief Hosted Inter-Process Communication (IPC) resources, shared memory,
/// message queues, semaphores, local UNIX domain sockets, and IPC limits.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms, Android, and OpenHarmony).
/// @note Apple mobile sandboxes expose no acceptable system-wide IPC
/// inventory or limit source to this interface, so all queries report
/// not_supported.
/// @note This module exposes:
/// - Enumeration of POSIX and System V shared memory segments
/// (shared_memory_segments()).
/// - Enumeration of POSIX and System V message queues (message_queues()).
/// - Enumeration of POSIX and System V semaphore sets (semaphore_sets()).
/// - Enumeration of active local UNIX domain sockets (local_sockets()).
/// - System-wide or namespace-wide IPC limits and tuning parameters (limits()).
/// @note Linux queries procfs (/proc/sysvipc/shm, /proc/sysvipc/msg,
/// /proc/sysvipc/sem, /proc/net/unix, /proc/[pid]/fd) and standard filesystem
/// locations (/dev/shm, /dev/mqueue). Results represent a point-in-time runtime
/// snapshot visible to the calling process within its current execution
/// namespace(s) (IPC, network, mount, and PID namespaces). Because /dev/shm is
/// a standard tmpfs mount, regular files created by applications in /dev/shm
/// are enumerated as candidate POSIX shared memory objects.
/// @note Process IDs associated with local sockets reflect observable processes
/// holding open file descriptors, subject to operating-system caller
/// permissions.
/// @note FreeBSD exposes System V IPC limits through documented sysctl values;
/// object inventories and local sockets report not_supported. AIX, HP-UX,
/// GNU/Hurd, SerenityOS, and Redox OS report not_supported for IPC enumeration.
/// Windows and macOS targets use their respective platform mechanisms or
/// generic fallbacks.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/ipc.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <syscape/detail/ipc/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__) && !defined(SYSCAPE_TARGET_OPENHARMONY) &&           \
    !defined(SYSCAPE_TARGET_AIX) && !defined(SYSCAPE_TARGET_HPUX) &&           \
    !defined(SYSCAPE_TARGET_HURD) && !defined(SYSCAPE_TARGET_SERENITY) &&      \
    !defined(SYSCAPE_TARGET_REDOX)
#include <syscape/detail/ipc/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/ipc/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/ipc/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/ipc/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/ipc/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/ipc/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/ipc/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/ipc/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/ipc/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/ipc/openharmony.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/ipc/solaris.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__HAIKU__)
#include <syscape/detail/ipc/haiku.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_AIX)
#include <syscape/detail/ipc/aix.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_HPUX)
#include <syscape/detail/ipc/hpux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_HURD)
#include <syscape/detail/ipc/hurd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_SERENITY)
#include <syscape/detail/ipc/serenity.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_REDOX)
#include <syscape/detail/ipc/redox.hpp>
#else
#include <syscape/detail/ipc/generic.hpp>
#endif

namespace syscape {
namespace ipc {

/// Enumerates all accessible shared memory segments (both System V and POSIX)
/// visible to the calling process in its current IPC and mount namespaces.
///
/// Values represent a point-in-time runtime snapshot and can change dynamically.
/// @return A vector of shared_memory_segment structures sorted deterministically,
/// not_supported on unsupported platforms or when no underlying subsystems exist,
/// or a platform error code.
inline result<std::vector<shared_memory_segment>> shared_memory_segments() {
    return detail::ipc_backend::shared_memory_segments();
}

/// Enumerates all accessible message queues (both System V and POSIX)
/// visible to the calling process in its current IPC namespace.
///
/// Values represent a point-in-time runtime snapshot and can change dynamically.
/// @return A vector of message_queue structures sorted deterministically,
/// not_supported on unsupported platforms or when no underlying subsystems exist,
/// or a platform error code.
inline result<std::vector<message_queue>> message_queues() {
    return detail::ipc_backend::message_queues();
}

/// Enumerates all accessible semaphore sets and named semaphores (both System V and POSIX)
/// visible to the calling process in its current IPC and mount namespaces.
///
/// Values represent a point-in-time runtime snapshot and can change dynamically.
/// @return A vector of semaphore_set structures sorted deterministically,
/// not_supported on unsupported platforms or when no underlying subsystems exist,
/// or a platform error code.
inline result<std::vector<semaphore_set>> semaphore_sets() {
    return detail::ipc_backend::semaphore_sets();
}

/// Enumerates all active local UNIX domain sockets visible to the calling process
/// in its current network and mount namespaces.
///
/// Process IDs associated with sockets contain observable owning processes
/// discovering open descriptors referencing the socket inode, subject to permissions.
/// Values represent a point-in-time runtime snapshot and can change dynamically.
/// @return A vector of local_socket structures sorted deterministically,
/// not_supported on unsupported platforms, or a platform error code.
inline result<std::vector<local_socket>> local_sockets() {
    return detail::ipc_backend::local_sockets();
}

/// Queries IPC resource limits and kernel tuning parameters for the current namespace.
///
/// Values represent a point-in-time runtime snapshot and can change dynamically.
/// @return An ipc_limits structure containing observed limit values,
/// not_supported on unsupported platforms or when no limit sources exist,
/// or a platform error code.
inline result<ipc_limits> limits() {
    return detail::ipc_backend::limits();
}

} // namespace ipc
} // namespace syscape

#endif // SYSCAPE_IPC_HPP
