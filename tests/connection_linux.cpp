#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/types.h>
#include <unistd.h>

#include <syscape/connection.hpp>
#include <syscape/detail/connection/linux.hpp>

namespace {

void test_hex_ipv4_parsing() {
    std::array<unsigned char, 16> out {};
    const bool little =
        syscape::detail::connection_backend::linux_impl::host_is_little_endian();
    const char* const loopback = little ? "0100007F" : "7F000001";
    assert(syscape::detail::connection_backend::linux_impl::parse_hex_ipv4(loopback, out));
    assert(out[0] == 127);
    assert(out[1] == 0);
    assert(out[2] == 0);
    assert(out[3] == 1);

    // "00000000" -> 0.0.0.0
    assert(syscape::detail::connection_backend::linux_impl::parse_hex_ipv4("00000000", out));
    assert(out[0] == 0 && out[1] == 0 && out[2] == 0 && out[3] == 0);

    const char* const private_address = little ? "521FA8C0" : "C0A81F52";
    assert(syscape::detail::connection_backend::linux_impl::parse_hex_ipv4(private_address, out));
    assert(out[0] == 192);
    assert(out[1] == 168);
    assert(out[2] == 31);
    assert(out[3] == 82);

    // Invalid length or chars
    assert(!syscape::detail::connection_backend::linux_impl::parse_hex_ipv4("0100007", out));
    assert(!syscape::detail::connection_backend::linux_impl::parse_hex_ipv4("0100007FF", out));
    assert(!syscape::detail::connection_backend::linux_impl::parse_hex_ipv4("0100007G", out));
}

void test_hex_ipv6_parsing() {
    std::array<unsigned char, 16> out {};
    const bool little =
        syscape::detail::connection_backend::linux_impl::host_is_little_endian();
    const char* const loopback = little
                                     ? "00000000000000000000000001000000"
                                     : "00000000000000000000000000000001";
    assert(syscape::detail::connection_backend::linux_impl::parse_hex_ipv6(loopback, out));
    for (std::size_t i = 0; i < 15; ++i) {
        assert(out[i] == 0);
    }
    assert(out[15] == 1);

    // All zero address "::"
    assert(syscape::detail::connection_backend::linux_impl::parse_hex_ipv6("00000000000000000000000000000000", out));
    for (std::size_t i = 0; i < 16; ++i) {
        assert(out[i] == 0);
    }

    // Invalid lengths
    assert(!syscape::detail::connection_backend::linux_impl::parse_hex_ipv6("0000000000000000", out));
    assert(!syscape::detail::connection_backend::linux_impl::parse_hex_ipv6("0000000000000000000000000100000Z", out));
}

void test_endpoint_parsing() {
    syscape::detail::connection_common::socket_endpoint_record ep;
    const bool little =
        syscape::detail::connection_backend::linux_impl::host_is_little_endian();
    const std::string endpoint =
        std::string(little ? "0100007F" : "7F000001") + ":1F90";
    // 127.0.0.1:8080 -> port 8080 = 0x1F90
    assert(syscape::detail::connection_backend::linux_impl::parse_endpoint(
        endpoint, syscape::detail::connection_common::address_family::ipv4, ep));
    assert(ep.address.family == syscape::detail::connection_common::address_family::ipv4);
    assert(ep.address.value[0] == 127 && ep.address.value[3] == 1);
    assert(ep.port == 8080);

    // Invalid port > 65535
    assert(!syscape::detail::connection_backend::linux_impl::parse_endpoint(
        "0100007F:10000", syscape::detail::connection_common::address_family::ipv4, ep));

    // Missing colon
    assert(!syscape::detail::connection_backend::linux_impl::parse_endpoint(
        "0100007F1F90", syscape::detail::connection_common::address_family::ipv4, ep));
}

void test_tcp_state_mapping() {
    using syscape::detail::connection_common::tcp_state;
    using syscape::detail::connection_backend::linux_impl::map_linux_tcp_state;

    assert(map_linux_tcp_state(1) == tcp_state::established);
    assert(map_linux_tcp_state(2) == tcp_state::syn_sent);
    assert(map_linux_tcp_state(3) == tcp_state::syn_recv);
    assert(map_linux_tcp_state(4) == tcp_state::fin_wait1);
    assert(map_linux_tcp_state(5) == tcp_state::fin_wait2);
    assert(map_linux_tcp_state(6) == tcp_state::time_wait);
    assert(map_linux_tcp_state(7) == tcp_state::closed);
    assert(map_linux_tcp_state(8) == tcp_state::close_wait);
    assert(map_linux_tcp_state(9) == tcp_state::last_ack);
    assert(map_linux_tcp_state(10) == tcp_state::listen);
    assert(map_linux_tcp_state(11) == tcp_state::closing);
    assert(map_linux_tcp_state(0) == tcp_state::unknown);
    assert(map_linux_tcp_state(99) == tcp_state::unknown);
}

void test_synthetic_proc_net_line_parsing() {
    syscape::detail::connection_backend::linux_impl::inode_pid_map_type map;
    map[667220] = {1234};

    syscape::detail::connection_common::connection_record rec;

    // Standard listening TCP line
    const bool little =
        syscape::detail::connection_backend::linux_impl::host_is_little_endian();
    const std::string line_listen =
        std::string("   0: ") + (little ? "0100007F" : "7F000001") +
        ":23FA 00000000:0000 0A 00000000:00000000 00:00000000 "
        "00000000  1000        0 667220 1 00000000cf66c08c 100 0 0 10 0";
    assert(syscape::detail::connection_backend::linux_impl::parse_proc_net_line(
        line_listen, syscape::detail::connection_common::protocol::tcp,
        syscape::detail::connection_common::address_family::ipv4, map, rec));

    assert(rec.transport_protocol == syscape::detail::connection_common::protocol::tcp);
    assert(rec.local_endpoint.address.value[0] == 127);
    assert(rec.local_endpoint.port == 0x23FA);
    assert(!rec.remote_endpoint.has_value());
    assert(rec.state == syscape::detail::connection_common::tcp_state::listen);
    assert(rec.send_queue_bytes.has_value() && *rec.send_queue_bytes == 0);
    assert(rec.receive_queue_bytes.has_value() && *rec.receive_queue_bytes == 0);
    assert(rec.uid.has_value() && *rec.uid == 1000);
    assert(rec.inode.has_value() && *rec.inode == 667220);
    assert(rec.pid.has_value() && *rec.pid == 1234);

    // Established TCP line with remote endpoint and queue values
    const std::string line_estab =
        std::string("   1: ") + (little ? "521FA8C0" : "C0A81F52") +
        ":0050 " + (little ? "011FA8C0" : "C0A81F01") +
        ":1F90 01 00000020:00000040 00:00000000 00000000     0        0 "
        "999999 1 000000005866cf02 100 0 0 10 0";
    assert(syscape::detail::connection_backend::linux_impl::parse_proc_net_line(
        line_estab, syscape::detail::connection_common::protocol::tcp,
        syscape::detail::connection_common::address_family::ipv4, map, rec));

    assert(rec.state == syscape::detail::connection_common::tcp_state::established);
    assert(rec.remote_endpoint.has_value());
    assert(rec.remote_endpoint->address.value[0] == 192);
    assert(rec.remote_endpoint->port == 8080);
    assert(rec.send_queue_bytes.has_value() && *rec.send_queue_bytes == 0x20);
    assert(rec.receive_queue_bytes.has_value() && *rec.receive_queue_bytes == 0x40);
    assert(rec.uid.has_value() && *rec.uid == 0);
    assert(rec.inode.has_value() && *rec.inode == 999999);
    // Inode 999999 was not in map, so pid should be nullopt
    assert(!rec.pid.has_value());

    // Corrupt line (missing fields)
    assert(!syscape::detail::connection_backend::linux_impl::parse_proc_net_line(
        "   0: 0100007F:23FA", syscape::detail::connection_common::protocol::tcp,
        syscape::detail::connection_common::address_family::ipv4, map, rec));

    const std::string malformed_queue =
        "   0: 0100007F:23FA 00000000:0000 0A invalid 00:00000000 "
        "00000000 1000 0 667220";
    assert(!syscape::detail::connection_backend::linux_impl::parse_proc_net_line(
        malformed_queue, syscape::detail::connection_common::protocol::tcp,
        syscape::detail::connection_common::address_family::ipv4, map, rec));
}

void test_shared_socket_owners() {
    using namespace syscape::detail;
    connection_backend::linux_impl::inode_pid_map_type owners;
    owners[42] = {7, 9};

    connection_common::connection_record record;
    record.inode = 42;
    std::vector<connection_common::connection_record> expanded;
    connection_backend::linux_impl::append_record_for_owners(record, owners, expanded);

    assert(expanded.size() == 2);
    assert(expanded[0].pid.has_value() && *expanded[0].pid == 7);
    assert(expanded[1].pid.has_value() && *expanded[1].pid == 9);
}

void test_scope_id_ordering() {
    using namespace syscape::detail::connection_common;
    connection_record first;
    connection_record second;
    first.local_endpoint.address.family = address_family::ipv6;
    second.local_endpoint.address.family = address_family::ipv6;
    first.local_endpoint.address.scope_id = 2;
    second.local_endpoint.address.scope_id = 3;
    assert(compare_connection_records(first, second));
    assert(!compare_connection_records(second, first));
}

void test_live_queries() {
    const auto all = syscape::connection::connections();
    if (!all) {
        std::cout << "Live connections() query returned error: " << all.error().message() << std::endl;
        return;
    }

    std::cout << "Discovered " << all->size() << " network connections/sockets." << std::endl;

    for (const auto& conn : *all) {
        // Basic sanity assertions
        assert(conn.local_endpoint.port <= 65535);
        if (conn.remote_endpoint) {
            assert(conn.remote_endpoint->port <= 65535);
        }
    }

    const auto tcp_list = syscape::connection::tcp_connections();
    assert(tcp_list.has_value());
    for (const auto& conn : *tcp_list) {
        assert(conn.transport_protocol == syscape::connection::protocol::tcp);
    }

    const auto udp_list = syscape::connection::udp_endpoints();
    assert(udp_list.has_value());
    for (const auto& conn : *udp_list) {
        assert(conn.transport_protocol == syscape::connection::protocol::udp);
        assert(conn.state == syscape::connection::tcp_state::unknown);
    }

    const auto listen_list = syscape::connection::listening_endpoints();
    assert(listen_list.has_value());
    for (const auto& conn : *listen_list) {
        if (conn.transport_protocol == syscape::connection::protocol::tcp) {
            assert(conn.state == syscape::connection::tcp_state::listen);
        } else {
            assert(!conn.remote_endpoint.has_value());
        }
    }

    // Query for current process PID
    const pid_t my_pid = ::getpid();
    const auto my_conns = syscape::connection::find_connections_by_process(static_cast<std::uint32_t>(my_pid));
    assert(my_conns.has_value());
    for (const auto& conn : *my_conns) {
        assert(conn.pid.has_value() && *conn.pid == static_cast<std::uint32_t>(my_pid));
    }
}

} // namespace

int main() {
    test_hex_ipv4_parsing();
    test_hex_ipv6_parsing();
    test_endpoint_parsing();
    test_tcp_state_mapping();
    test_synthetic_proc_net_line_parsing();
    test_shared_socket_owners();
    test_scope_id_ordering();
    test_live_queries();
    return 0;
}
