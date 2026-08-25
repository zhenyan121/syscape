#ifndef SYSCAPE_DETAIL_CONNECTION_COMMON_HPP
#define SYSCAPE_DETAIL_CONNECTION_COMMON_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <syscape/detail/config.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace connection_common {

/// Transport protocol of the connection or socket.
enum class protocol : std::uint8_t {
    tcp,
    udp
};

/// RFC 793 / operating system TCP connection states.
enum class tcp_state : std::uint8_t {
    unknown = 0,
    established = 1,
    syn_sent = 2,
    syn_recv = 3,
    fin_wait1 = 4,
    fin_wait2 = 5,
    time_wait = 6,
    closed = 7,
    close_wait = 8,
    last_ack = 9,
    listen = 10,
    closing = 11
};

/// Address family of an endpoint (IPv4 / IPv6).
enum class address_family : std::uint8_t {
    ipv4,
    ipv6
};

/// IP address representation in network byte order.
struct ip_address_record {
    address_family family = address_family::ipv4;
    std::array<unsigned char, 16> value {};
    std::uint32_t scope_id = 0;
};

/// A network socket endpoint consisting of an IP address and port.
struct socket_endpoint_record {
    ip_address_record address;
    std::uint16_t port = 0;
};

/// Observable metadata describing an active socket or connection.
struct connection_record {
    protocol transport_protocol = protocol::tcp;
    socket_endpoint_record local_endpoint;
    std::optional<socket_endpoint_record> remote_endpoint;
    tcp_state state = tcp_state::unknown;
    std::optional<std::uint32_t> pid;
    std::optional<std::uint32_t> uid;
    std::optional<std::uint64_t> inode;
    std::optional<std::uint32_t> send_queue_bytes;
    std::optional<std::uint32_t> receive_queue_bytes;
};

/// Compares two IP address records for ordering.
inline bool compare_ip_addresses(
    const ip_address_record& lhs,
    const ip_address_record& rhs) noexcept {
    if (lhs.family != rhs.family) {
        return lhs.family < rhs.family;
    }
    if (lhs.value != rhs.value) {
        return lhs.value < rhs.value;
    }
    return lhs.scope_id < rhs.scope_id;
}

/// Compares two socket endpoint records for ordering.
inline bool compare_endpoints(
    const socket_endpoint_record& lhs,
    const socket_endpoint_record& rhs) noexcept {
    if (lhs.address.family != rhs.address.family ||
        lhs.address.value != rhs.address.value ||
        lhs.address.scope_id != rhs.address.scope_id) {
        return compare_ip_addresses(lhs.address, rhs.address);
    }
    return lhs.port < rhs.port;
}

/// Compares two connection records for deterministic ordering.
inline bool compare_connection_records(
    const connection_record& lhs,
    const connection_record& rhs) noexcept {
    if (lhs.transport_protocol != rhs.transport_protocol) {
        return lhs.transport_protocol < rhs.transport_protocol;
    }
    if (lhs.local_endpoint.port != rhs.local_endpoint.port ||
        lhs.local_endpoint.address.family != rhs.local_endpoint.address.family ||
        lhs.local_endpoint.address.value != rhs.local_endpoint.address.value ||
        lhs.local_endpoint.address.scope_id != rhs.local_endpoint.address.scope_id) {
        return compare_endpoints(lhs.local_endpoint, rhs.local_endpoint);
    }
    const bool lhs_has_remote = lhs.remote_endpoint.has_value();
    const bool rhs_has_remote = rhs.remote_endpoint.has_value();
    if (lhs_has_remote != rhs_has_remote) {
        return !lhs_has_remote && rhs_has_remote;
    }
    if (lhs_has_remote && rhs_has_remote) {
        if (lhs.remote_endpoint->port != rhs.remote_endpoint->port ||
            lhs.remote_endpoint->address.family != rhs.remote_endpoint->address.family ||
            lhs.remote_endpoint->address.value != rhs.remote_endpoint->address.value ||
            lhs.remote_endpoint->address.scope_id != rhs.remote_endpoint->address.scope_id) {
            return compare_endpoints(*lhs.remote_endpoint, *rhs.remote_endpoint);
        }
    }
    if (lhs.state != rhs.state) {
        return static_cast<std::uint8_t>(lhs.state) < static_cast<std::uint8_t>(rhs.state);
    }
    const std::uint32_t lhs_pid = lhs.pid.value_or(0);
    const std::uint32_t rhs_pid = rhs.pid.value_or(0);
    if (lhs_pid != rhs_pid) {
        return lhs_pid < rhs_pid;
    }
    const std::uint64_t lhs_inode = lhs.inode.value_or(0);
    const std::uint64_t rhs_inode = rhs.inode.value_or(0);
    return lhs_inode < rhs_inode;
}

/// Sorts connection records deterministically.
inline void sort_connection_records(std::vector<connection_record>& records) {
    std::sort(records.begin(), records.end(), compare_connection_records);
}

} // namespace connection_common
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_CONNECTION_COMMON_HPP
