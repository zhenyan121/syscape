#ifndef SYSCAPE_NUMA_HPP
#define SYSCAPE_NUMA_HPP

/// @file
/// @brief Hosted Non-Uniform Memory Access (NUMA) topology, CPU and memory
/// node mapping, node distance matrix, and thread node queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux implements every query through kernel-documented sysfs
/// interfaces under /sys/devices/system/node/ and the getcpu system call.
/// Windows implements NUMA queries using Windows 7 or later NUMA APIs
/// (_WIN32_WINNT >= 0x0601, including GetNumaHighestNodeNumber,
/// GetNumaNodeProcessorMaskEx, GetNumaNodeProcessorMask2 when available,
/// GetNumaAvailableMemoryNodeEx, GetCurrentProcessorNumberEx, and
/// GetNumaProcessorNodeEx). macOS operates on Uniform Memory Access (UMA)
/// architectures and reports a single unified node or unsupported. Other
/// targets use the not-supported generic fallback.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/numa.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <optional>
#include <vector>

#include <syscape/detail/numa/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/numa/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/numa/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/numa/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/numa/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/numa/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/numa/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/numa/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/numa/android.hpp>
#else
#include <syscape/detail/numa/generic.hpp>
#endif

namespace syscape {
namespace numa {

/// Checks whether NUMA architecture is available and multi-node (i.e. more than 1 node).
///
/// On systems with Uniform Memory Access (UMA) or a single memory controller,
/// this returns false while node_count() returns 1.
/// @return True if multiple NUMA nodes exist, false on single-node/UMA systems,
/// or not_supported when the platform exposes no acceptable source.
inline result<bool> is_numa_available() {
    return detail::numa_backend::is_numa_available();
}

/// Returns the number of NUMA nodes in the system.
///
/// On UMA systems this returns 1.
/// @return A positive count of NUMA nodes, not_supported on unsupported targets,
/// or a native error.
inline result<std::uint32_t> node_count() {
    return detail::numa_backend::node_count();
}

/// Enumerates all NUMA nodes in the system.
///
/// Nodes are returned sorted in ascending order by node id.
/// @return A vector of numa_node structures, not_supported on unsupported targets,
/// or a native error.
inline result<std::vector<numa_node>> nodes() {
    return detail::numa_backend::nodes();
}

/// Queries a specific NUMA node by its numeric identifier.
///
/// @param id The zero-based node ID to look up.
/// @return The matching numa_node structure, errc::not_found if no node with
/// that ID exists, not_supported on unsupported targets, or a native error.
inline result<numa_node> node(std::uint32_t id) {
    return detail::numa_backend::node(id);
}

/// Returns the NUMA node ID on which the calling thread is currently executing.
///
/// @return The zero-based NUMA node ID, not_supported on unsupported targets,
/// or a native error.
inline result<std::uint32_t> current_thread_node() {
    return detail::numa_backend::current_thread_node();
}

} // namespace numa
} // namespace syscape

#endif // SYSCAPE_NUMA_HPP
