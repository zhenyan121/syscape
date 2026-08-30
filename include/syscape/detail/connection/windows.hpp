#ifndef SYSCAPE_DETAIL_CONNECTION_WINDOWS_HPP
#define SYSCAPE_DETAIL_CONNECTION_WINDOWS_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <system_error>
#include <vector>

#include <syscape/detail/connection/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace connection_backend {

namespace windows_impl {

inline connection_common::tcp_state map_windows_tcp_state(DWORD st) noexcept {
    switch (st) {
    case MIB_TCP_STATE_CLOSED: return connection_common::tcp_state::closed;
    case MIB_TCP_STATE_LISTEN: return connection_common::tcp_state::listen;
    case MIB_TCP_STATE_SYN_SENT: return connection_common::tcp_state::syn_sent;
    case MIB_TCP_STATE_SYN_RCVD:
        return connection_common::tcp_state::syn_recv;
    case MIB_TCP_STATE_ESTAB: return connection_common::tcp_state::established;
    case MIB_TCP_STATE_FIN_WAIT1: return connection_common::tcp_state::fin_wait1;
    case MIB_TCP_STATE_FIN_WAIT2: return connection_common::tcp_state::fin_wait2;
    case MIB_TCP_STATE_CLOSE_WAIT: return connection_common::tcp_state::close_wait;
    case MIB_TCP_STATE_CLOSING: return connection_common::tcp_state::closing;
    case MIB_TCP_STATE_LAST_ACK: return connection_common::tcp_state::last_ack;
    case MIB_TCP_STATE_TIME_WAIT: return connection_common::tcp_state::time_wait;
    case MIB_TCP_STATE_DELETE_TCB: return connection_common::tcp_state::closed;
    default: return connection_common::tcp_state::unknown;
    }
}

inline connection_common::ip_address_record make_ipv4_address(DWORD addr_net) noexcept {
    connection_common::ip_address_record rec;
    rec.family = connection_common::address_family::ipv4;
    rec.scope_id = 0;
    rec.value[0] = static_cast<unsigned char>(addr_net & 0xFFU);
    rec.value[1] = static_cast<unsigned char>((addr_net >> 8U) & 0xFFU);
    rec.value[2] = static_cast<unsigned char>((addr_net >> 16U) & 0xFFU);
    rec.value[3] = static_cast<unsigned char>((addr_net >> 24U) & 0xFFU);
    return rec;
}

inline connection_common::ip_address_record make_ipv6_address(const UCHAR bytes[16], DWORD scope_id) noexcept {
    connection_common::ip_address_record rec;
    rec.family = connection_common::address_family::ipv6;
    rec.scope_id = static_cast<std::uint32_t>(scope_id);
    for (std::size_t i = 0; i < 16; ++i) {
        rec.value[i] = bytes[i];
    }
    return rec;
}

inline bool is_ipv4_zero(DWORD addr) noexcept {
    return addr == 0;
}

inline bool is_ipv6_zero(const UCHAR bytes[16]) noexcept {
    for (std::size_t i = 0; i < 16; ++i) {
        if (bytes[i] != 0) {
            return false;
        }
    }
    return true;
}

inline bool is_unsupported_table_error(const std::error_code& error) noexcept {
    return error == std::error_code(
                        ERROR_NOT_SUPPORTED, std::system_category());
}

template <typename Query>
inline result<std::vector<unsigned char>> query_dynamic_table(Query&& query) {
    DWORD size = 0;
    DWORD ret = query(nullptr, &size);
    if (ret != ERROR_INSUFFICIENT_BUFFER || size == 0) {
        if (ret == NO_ERROR) {
            return fail(errc::malformed_data);
        }
        return fail(std::error_code(static_cast<int>(ret), std::system_category()));
    }

    for (unsigned int attempt = 0; attempt < 8U; ++attempt) {
        std::vector<unsigned char> buffer(size);
        ret = query(buffer.data(), &size);
        if (ret == NO_ERROR) {
            if (size > buffer.size()) {
                return fail(errc::malformed_data);
            }
            buffer.resize(size);
            return buffer;
        }
        if (ret != ERROR_INSUFFICIENT_BUFFER || size == 0) {
            return fail(std::error_code(static_cast<int>(ret), std::system_category()));
        }
    }

    return fail(errc::temporarily_unavailable);
}

template <typename Table, typename Row>
inline bool table_has_valid_size(
    const std::vector<unsigned char>& buffer) noexcept {
    const std::size_t header_size = offsetof(Table, table);
    if (buffer.size() < header_size) {
        return false;
    }
    const auto* table = reinterpret_cast<const Table*>(buffer.data());
    return static_cast<std::size_t>(table->dwNumEntries) <=
           (buffer.size() - header_size) / sizeof(Row);
}

inline result<void> query_tcp4_table(
    std::vector<connection_common::connection_record>& records) {
    const auto buffer_result = query_dynamic_table([](PVOID buffer, PDWORD size) {
        return ::GetExtendedTcpTable(
            buffer, size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    });
    if (!buffer_result) {
        return fail(buffer_result.error());
    }
    const auto* table =
        reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(buffer_result->data());
    if (!table_has_valid_size<MIB_TCPTABLE_OWNER_PID, MIB_TCPROW_OWNER_PID>(
            *buffer_result)) {
        return fail(errc::malformed_data);
    }
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const MIB_TCPROW_OWNER_PID& row = table->table[i];
        connection_common::connection_record rec;
        rec.transport_protocol = connection_common::protocol::tcp;
        rec.local_endpoint.address = make_ipv4_address(row.dwLocalAddr);
        rec.local_endpoint.port = ntohs(static_cast<u_short>(row.dwLocalPort));
        if (is_ipv4_zero(row.dwRemoteAddr) && row.dwRemotePort == 0) {
            rec.remote_endpoint = std::nullopt;
        } else {
            connection_common::socket_endpoint_record remote;
            remote.address = make_ipv4_address(row.dwRemoteAddr);
            remote.port = ntohs(static_cast<u_short>(row.dwRemotePort));
            rec.remote_endpoint = remote;
        }
        rec.state = map_windows_tcp_state(row.dwState);
        if (row.dwOwningPid > 0) {
            rec.pid = static_cast<std::uint32_t>(row.dwOwningPid);
        }
        records.push_back(rec);
    }
    return {};
}

inline result<void> query_tcp6_table(
    std::vector<connection_common::connection_record>& records) {
    const auto buffer_result = query_dynamic_table([](PVOID buffer, PDWORD size) {
        return ::GetExtendedTcpTable(
            buffer, size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
    });
    if (!buffer_result) {
        return fail(buffer_result.error());
    }
    const auto* table =
        reinterpret_cast<const MIB_TCP6TABLE_OWNER_PID*>(buffer_result->data());
    if (!table_has_valid_size<MIB_TCP6TABLE_OWNER_PID, MIB_TCP6ROW_OWNER_PID>(
            *buffer_result)) {
        return fail(errc::malformed_data);
    }
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const MIB_TCP6ROW_OWNER_PID& row = table->table[i];
        connection_common::connection_record rec;
        rec.transport_protocol = connection_common::protocol::tcp;
        rec.local_endpoint.address = make_ipv6_address(row.ucLocalAddr, row.dwLocalScopeId);
        rec.local_endpoint.port = ntohs(static_cast<u_short>(row.dwLocalPort));
        if (is_ipv6_zero(row.ucRemoteAddr) && row.dwRemotePort == 0) {
            rec.remote_endpoint = std::nullopt;
        } else {
            connection_common::socket_endpoint_record remote;
            remote.address = make_ipv6_address(row.ucRemoteAddr, row.dwRemoteScopeId);
            remote.port = ntohs(static_cast<u_short>(row.dwRemotePort));
            rec.remote_endpoint = remote;
        }
        rec.state = map_windows_tcp_state(row.dwState);
        if (row.dwOwningPid > 0) {
            rec.pid = static_cast<std::uint32_t>(row.dwOwningPid);
        }
        records.push_back(rec);
    }
    return {};
}

inline result<void> query_udp4_table(
    std::vector<connection_common::connection_record>& records) {
    const auto buffer_result = query_dynamic_table([](PVOID buffer, PDWORD size) {
        return ::GetExtendedUdpTable(
            buffer, size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    });
    if (!buffer_result) {
        return fail(buffer_result.error());
    }
    const auto* table =
        reinterpret_cast<const MIB_UDPTABLE_OWNER_PID*>(buffer_result->data());
    if (!table_has_valid_size<MIB_UDPTABLE_OWNER_PID, MIB_UDPROW_OWNER_PID>(
            *buffer_result)) {
        return fail(errc::malformed_data);
    }
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const MIB_UDPROW_OWNER_PID& row = table->table[i];
        connection_common::connection_record rec;
        rec.transport_protocol = connection_common::protocol::udp;
        rec.local_endpoint.address = make_ipv4_address(row.dwLocalAddr);
        rec.local_endpoint.port = ntohs(static_cast<u_short>(row.dwLocalPort));
        rec.remote_endpoint = std::nullopt;
        rec.state = connection_common::tcp_state::unknown;
        if (row.dwOwningPid > 0) {
            rec.pid = static_cast<std::uint32_t>(row.dwOwningPid);
        }
        records.push_back(rec);
    }
    return {};
}

inline result<void> query_udp6_table(
    std::vector<connection_common::connection_record>& records) {
    const auto buffer_result = query_dynamic_table([](PVOID buffer, PDWORD size) {
        return ::GetExtendedUdpTable(
            buffer, size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
    });
    if (!buffer_result) {
        return fail(buffer_result.error());
    }
    const auto* table =
        reinterpret_cast<const MIB_UDP6TABLE_OWNER_PID*>(buffer_result->data());
    if (!table_has_valid_size<MIB_UDP6TABLE_OWNER_PID, MIB_UDP6ROW_OWNER_PID>(
            *buffer_result)) {
        return fail(errc::malformed_data);
    }
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const MIB_UDP6ROW_OWNER_PID& row = table->table[i];
        connection_common::connection_record rec;
        rec.transport_protocol = connection_common::protocol::udp;
        rec.local_endpoint.address = make_ipv6_address(row.ucLocalAddr, row.dwLocalScopeId);
        rec.local_endpoint.port = ntohs(static_cast<u_short>(row.dwLocalPort));
        rec.remote_endpoint = std::nullopt;
        rec.state = connection_common::tcp_state::unknown;
        if (row.dwOwningPid > 0) {
            rec.pid = static_cast<std::uint32_t>(row.dwOwningPid);
        }
        records.push_back(rec);
    }
    return {};
}

} // namespace windows_impl

inline result<std::vector<connection_common::connection_record>> connections() {
    std::vector<connection_common::connection_record> records;
    bool any_table_supported = false;

    const result<void> tcp4 = windows_impl::query_tcp4_table(records);
    if (tcp4) {
        any_table_supported = true;
    } else if (!windows_impl::is_unsupported_table_error(tcp4.error())) {
        return fail(tcp4.error());
    }
    const result<void> tcp6 = windows_impl::query_tcp6_table(records);
    if (tcp6) {
        any_table_supported = true;
    } else if (!windows_impl::is_unsupported_table_error(tcp6.error())) {
        return fail(tcp6.error());
    }
    const result<void> udp4 = windows_impl::query_udp4_table(records);
    if (udp4) {
        any_table_supported = true;
    } else if (!windows_impl::is_unsupported_table_error(udp4.error())) {
        return fail(udp4.error());
    }
    const result<void> udp6 = windows_impl::query_udp6_table(records);
    if (udp6) {
        any_table_supported = true;
    } else if (!windows_impl::is_unsupported_table_error(udp6.error())) {
        return fail(udp6.error());
    }
    if (!any_table_supported) {
        return fail(errc::not_supported);
    }
    connection_common::sort_connection_records(records);
    return records;
}

} // namespace connection_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_CONNECTION_WINDOWS_HPP
