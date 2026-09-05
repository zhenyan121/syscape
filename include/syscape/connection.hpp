#ifndef SYSCAPE_CONNECTION_HPP
#define SYSCAPE_CONNECTION_HPP

/// @file
/// @brief Hosted network connection and socket inventory queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms, Android, and OpenHarmony).
/// @note Apple mobile sandboxes do not permit system-wide socket ownership
/// inventory through the public interfaces used here, so all queries report
/// not_supported.
/// @note This module exposes:
/// - Enumeration of all visible network connections and sockets
/// (connections()).
/// - Active and listening TCP connections (tcp_connections()).
/// - Visible UDP endpoints (udp_endpoints()).
/// - Listening endpoints (listening_endpoints()).
/// - Connections owned by a specific process
/// (find_connections_by_process(pid)).
/// - Transport protocol (TCP or UDP).
/// - Bound local endpoint and remote endpoint (IPv4 / IPv6 addresses and
/// ports).
/// - TCP connection lifecycle state (listen, established, time_wait,
/// close_wait, etc.).
/// - Owning process identifier (PID), user identifier (UID), and Linux socket
/// inode.
/// - Kernel transmit (TX) and receive (RX) buffer queue occupancy in bytes.
/// @note Linux parses /proc/net/{tcp,tcp6,udp,udp6} and correlates socket
/// inodes to PIDs via /proc/[pid]/fd without spawning external utilities.
/// @note Windows queries GetExtendedTcpTable and GetExtendedUdpTable from
/// iphlpapi.h. Applications that call these queries on Windows must link
/// Iphlpapi.lib. The supplied CMake tests demonstrate this SDK linkage without
/// imposing it on unrelated modules.
/// @note macOS queries Darwin libproc socket inspection APIs.
/// @note AIX and HP-UX report not_supported for socket and connection
/// inspection.
/// @note Network connections change continuously. Queries do not cache results.
/// Unprivileged callers receive partial observable metadata without failing the
/// whole snapshot.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/connection.hpp requires C++17 or later"
#endif

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <syscape/detail/connection/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__) && !defined(SYSCAPE_TARGET_OPENHARMONY) &&           \
    !defined(SYSCAPE_TARGET_AIX) && !defined(SYSCAPE_TARGET_HPUX)
#include <syscape/detail/connection/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/connection/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/connection/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/connection/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/connection/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/connection/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/connection/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/connection/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/connection/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/connection/openharmony.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/connection/solaris.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__HAIKU__)
#include <syscape/detail/connection/haiku.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_AIX)
#include <syscape/detail/connection/aix.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_HPUX)
#include <syscape/detail/connection/hpux.hpp>
#else
#include <syscape/detail/connection/generic.hpp>
#endif

namespace syscape {
namespace connection {

/// Transport protocol of the connection or socket.
using protocol = detail::connection_common::protocol;

/// RFC 793 / operating system TCP connection states.
using tcp_state = detail::connection_common::tcp_state;

/// Address family of an endpoint (IPv4 / IPv6).
using address_family = detail::connection_common::address_family;

/// IP address representation in network byte order.
struct ip_address {
    /// Address family selecting whether the first 4 or all 16 bytes are meaningful.
    address_family family = address_family::ipv4;

    /// Address octets in network byte order. IPv4 occupies the first 4 bytes.
    std::array<unsigned char, 16> value {};

    /// Numeric IPv6 scope identifier, or zero when unrecorded.
    std::uint32_t scope_id = 0;
};

/// A network socket endpoint consisting of an IP address and port number.
struct socket_endpoint {
    /// IP address bound or connected by the endpoint.
    ip_address address;

    /// Port number in host byte order (1..65535, or 0 for unassigned).
    std::uint16_t port = 0;
};

/// Observable metadata describing an active socket or connection.
struct connection_entry {
    /// Transport protocol (TCP or UDP).
    protocol transport_protocol = protocol::tcp;

    /// Local bound endpoint (IP address and port).
    socket_endpoint local_endpoint;

    /// Remote endpoint, or no value for listening or unconnected sockets.
    std::optional<socket_endpoint> remote_endpoint;

    /// TCP operational state, or unknown for connectionless protocols.
    tcp_state state = tcp_state::unknown;

    /// Owning process ID if visible to the caller, or no value.
    std::optional<std::uint32_t> pid;

    /// Owning user ID (UID on POSIX), or no value when unobservable.
    std::optional<std::uint32_t> uid;

    /// Platform-specific socket inode number on Linux, or no value.
    std::optional<std::uint64_t> inode;

    /// Number of bytes queued for transmission in the kernel send buffer.
    std::optional<std::uint32_t> send_queue_bytes;

    /// Number of bytes queued for reception in the kernel receive buffer.
    std::optional<std::uint32_t> receive_queue_bytes;
};

namespace detail_impl {

inline ip_address make_public_ip_address(
    const detail::connection_common::ip_address_record& rec) noexcept {
    ip_address addr;
    addr.family = rec.family;
    addr.value = rec.value;
    addr.scope_id = rec.scope_id;
    return addr;
}

inline socket_endpoint make_public_endpoint(
    const detail::connection_common::socket_endpoint_record& rec) noexcept {
    socket_endpoint ep;
    ep.address = make_public_ip_address(rec.address);
    ep.port = rec.port;
    return ep;
}

inline connection_entry make_public_entry(
    detail::connection_common::connection_record&& rec) {
    connection_entry entry;
    entry.transport_protocol = rec.transport_protocol;
    entry.local_endpoint = make_public_endpoint(rec.local_endpoint);
    if (rec.remote_endpoint) {
        entry.remote_endpoint = make_public_endpoint(*rec.remote_endpoint);
    } else {
        entry.remote_endpoint = std::nullopt;
    }
    entry.state = rec.state;
    entry.pid = rec.pid;
    entry.uid = rec.uid;
    entry.inode = rec.inode;
    entry.send_queue_bytes = rec.send_queue_bytes;
    entry.receive_queue_bytes = rec.receive_queue_bytes;
    return entry;
}

inline std::vector<connection_entry> convert_records(
    std::vector<detail::connection_common::connection_record>&& records) {
    std::vector<connection_entry> entries;
    entries.reserve(records.size());
    for (detail::connection_common::connection_record& rec : records) {
        entries.push_back(make_public_entry(std::move(rec)));
    }
    return entries;
}

} // namespace detail_impl

/// Returns a snapshot of all visible network connections and sockets.
///
/// @return A list of connection entries ordered deterministically, or an error code
/// on failure or when unsupported.
/// @note Returns native platform errors for access and I/O failures, malformed_data for
/// invalid platform records, and temporarily_unavailable for an unstable snapshot.
inline result<std::vector<connection_entry>> connections() {
    result<std::vector<detail::connection_common::connection_record>> raw =
        detail::connection_backend::connections();
    if (!raw) {
        return fail(raw.error());
    }
    return detail_impl::convert_records(std::move(*raw));
}

/// Returns a snapshot of visible TCP connections and listening sockets.
///
/// @return TCP entries, or the same errors documented by connections().
inline result<std::vector<connection_entry>> tcp_connections() {
    result<std::vector<detail::connection_common::connection_record>> raw =
        detail::connection_backend::connections();
    if (!raw) {
        return fail(raw.error());
    }
    std::vector<connection_entry> entries;
    for (detail::connection_common::connection_record& rec : *raw) {
        if (rec.transport_protocol == protocol::tcp) {
            entries.push_back(detail_impl::make_public_entry(std::move(rec)));
        }
    }
    return entries;
}

/// Returns a snapshot of visible UDP sockets and endpoints.
///
/// @return UDP entries, or the same errors documented by connections().
inline result<std::vector<connection_entry>> udp_endpoints() {
    result<std::vector<detail::connection_common::connection_record>> raw =
        detail::connection_backend::connections();
    if (!raw) {
        return fail(raw.error());
    }
    std::vector<connection_entry> entries;
    for (detail::connection_common::connection_record& rec : *raw) {
        if (rec.transport_protocol == protocol::udp) {
            entries.push_back(detail_impl::make_public_entry(std::move(rec)));
        }
    }
    return entries;
}

/// Returns a snapshot of all listening endpoints.
///
/// For TCP, returns connections in the listen state.
/// For UDP, returns endpoints without a connected remote endpoint.
/// @return Listening entries, or the same errors documented by connections().
inline result<std::vector<connection_entry>> listening_endpoints() {
    result<std::vector<detail::connection_common::connection_record>> raw =
        detail::connection_backend::connections();
    if (!raw) {
        return fail(raw.error());
    }
    std::vector<connection_entry> entries;
    for (detail::connection_common::connection_record& rec : *raw) {
        if (rec.transport_protocol == protocol::tcp) {
            if (rec.state == tcp_state::listen) {
                entries.push_back(detail_impl::make_public_entry(std::move(rec)));
            }
        } else if (rec.transport_protocol == protocol::udp) {
            if (!rec.remote_endpoint.has_value()) {
                entries.push_back(detail_impl::make_public_entry(std::move(rec)));
            }
        }
    }
    return entries;
}

/// Returns connections associated with a specific process ID.
///
/// A shared socket produces one entry for every observable owning process.
/// @return Matching entries, or the same errors documented by connections().
inline result<std::vector<connection_entry>> find_connections_by_process(std::uint32_t pid) {
    result<std::vector<detail::connection_common::connection_record>> raw =
        detail::connection_backend::connections();
    if (!raw) {
        return fail(raw.error());
    }
    std::vector<connection_entry> entries;
    for (detail::connection_common::connection_record& rec : *raw) {
        if (rec.pid.has_value() && *rec.pid == pid) {
            entries.push_back(detail_impl::make_public_entry(std::move(rec)));
        }
    }
    return entries;
}

} // namespace connection
} // namespace syscape

#endif // SYSCAPE_CONNECTION_HPP
