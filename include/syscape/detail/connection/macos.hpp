#ifndef SYSCAPE_DETAIL_CONNECTION_MACOS_HPP
#define SYSCAPE_DETAIL_CONNECTION_MACOS_HPP

#include <arpa/inet.h>
#include <libproc.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <sys/proc_info.h>
#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <set>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/detail/connection/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace connection_backend {

namespace macos_impl {

inline connection_common::tcp_state map_macos_tcp_state(int st) noexcept {
    switch (st) {
    case TCPS_CLOSED: return connection_common::tcp_state::closed;
    case TCPS_LISTEN: return connection_common::tcp_state::listen;
    case TCPS_SYN_SENT: return connection_common::tcp_state::syn_sent;
    case TCPS_SYN_RECEIVED: return connection_common::tcp_state::syn_recv;
    case TCPS_ESTABLISHED: return connection_common::tcp_state::established;
    case TCPS_CLOSE_WAIT: return connection_common::tcp_state::close_wait;
    case TCPS_FIN_WAIT_1: return connection_common::tcp_state::fin_wait1;
    case TCPS_CLOSING: return connection_common::tcp_state::closing;
    case TCPS_LAST_ACK: return connection_common::tcp_state::last_ack;
    case TCPS_FIN_WAIT_2: return connection_common::tcp_state::fin_wait2;
    case TCPS_TIME_WAIT: return connection_common::tcp_state::time_wait;
    default: return connection_common::tcp_state::unknown;
    }
}

inline connection_common::ip_address_record make_ipv4_address(in_addr_t addr_net) noexcept {
    connection_common::ip_address_record rec;
    rec.family = connection_common::address_family::ipv4;
    rec.scope_id = 0;
    std::memcpy(rec.value.data(), &addr_net, sizeof(addr_net));
    return rec;
}

inline connection_common::ip_address_record make_ipv6_address(
    const struct in6_addr& addr6,
    std::uint32_t scope_id) noexcept {
    connection_common::ip_address_record rec;
    rec.family = connection_common::address_family::ipv6;
    rec.scope_id = scope_id;
    std::memcpy(rec.value.data(), &addr6, sizeof(addr6));
    return rec;
}

inline bool is_ipv4_zero(in_addr_t addr_net) noexcept {
    return addr_net == 0;
}

inline bool is_ipv6_zero(const struct in6_addr& addr6) noexcept {
    for (std::size_t i = 0; i < 16; ++i) {
        if (reinterpret_cast<const unsigned char*>(&addr6)[i] != 0) {
            return false;
        }
    }
    return true;
}

} // namespace macos_impl

inline result<std::vector<connection_common::connection_record>> connections() {
    std::vector<connection_common::connection_record> records;

    errno = 0;
    const int required_bytes = ::proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (required_bytes < 0 || (required_bytes == 0 && errno != 0)) {
        return fail(std::error_code(errno != 0 ? errno : EIO,
                                    std::generic_category()));
    }
    if (required_bytes == 0) {
        return records;
    }

    const std::size_t required_count =
        (static_cast<std::size_t>(required_bytes) + sizeof(pid_t) - 1U) /
        sizeof(pid_t);
    std::vector<pid_t> pids(required_count + 64U);
    const std::size_t pid_buffer_bytes = pids.size() * sizeof(pid_t);
    if (pid_buffer_bytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return fail(errc::value_too_large);
    }
    errno = 0;
    const int actual_bytes = ::proc_listpids(
        PROC_ALL_PIDS, 0, pids.data(),
        static_cast<int>(pid_buffer_bytes));
    if (actual_bytes < 0 || (actual_bytes == 0 && errno != 0)) {
        return fail(std::error_code(errno != 0 ? errno : EIO,
                                    std::generic_category()));
    }
    if (actual_bytes == 0) {
        return records;
    }
    if (static_cast<std::size_t>(actual_bytes) > pid_buffer_bytes) {
        return fail(errc::temporarily_unavailable);
    }
    if (static_cast<std::size_t>(actual_bytes) % sizeof(pid_t) != 0U) {
        return fail(errc::malformed_data);
    }

    const std::size_t count = static_cast<std::size_t>(actual_bytes) / sizeof(pid_t);
    std::set<std::pair<std::uint32_t, std::uint64_t>> observed_sockets;
    for (std::size_t i = 0; i < count; ++i) {
        const pid_t pid = pids[i];
        if (pid <= 0) {
            continue;
        }

        const int buf_size = ::proc_pidinfo(pid, PROC_PIDLISTFDS, 0, nullptr, 0);
        if (buf_size <= 0) {
            continue;
        }

        if (static_cast<std::size_t>(buf_size) < sizeof(struct proc_fdinfo)) {
            continue;
        }
        const std::size_t fd_capacity =
            (static_cast<std::size_t>(buf_size) + sizeof(struct proc_fdinfo) - 1U) /
            sizeof(struct proc_fdinfo);
        std::vector<struct proc_fdinfo> fds(fd_capacity);
        const std::size_t fd_buffer_bytes = fds.size() * sizeof(struct proc_fdinfo);
        if (fd_buffer_bytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return fail(errc::value_too_large);
        }
        const int fds_bytes = ::proc_pidinfo(
            pid, PROC_PIDLISTFDS, 0, fds.data(),
            static_cast<int>(fd_buffer_bytes));
        if (fds_bytes <= 0) {
            continue;
        }
        if (static_cast<std::size_t>(fds_bytes) > fd_buffer_bytes) {
            return fail(errc::temporarily_unavailable);
        }
        if (static_cast<std::size_t>(fds_bytes) % sizeof(struct proc_fdinfo) != 0U) {
            return fail(errc::malformed_data);
        }

        const std::size_t num_fds = static_cast<std::size_t>(fds_bytes) / sizeof(struct proc_fdinfo);
        for (std::size_t f = 0; f < num_fds; ++f) {
            if (fds[f].proc_fdtype != PROX_FDTYPE_SOCKET) {
                continue;
            }

            struct socket_fdinfo sinfo;
            const int sinfo_bytes = ::proc_pidfdinfo(
                pid, fds[f].proc_fd, PROC_PIDFDSOCKETINFO, &sinfo, sizeof(sinfo));
            if (sinfo_bytes < static_cast<int>(sizeof(sinfo))) {
                continue;
            }

            const int family = sinfo.psi.soi_family;
            if (family != AF_INET && family != AF_INET6) {
                continue;
            }

            const int kind = sinfo.psi.soi_kind;
            const int proto = sinfo.psi.soi_protocol;

            const auto socket_identity = std::make_pair(
                static_cast<std::uint32_t>(pid),
                static_cast<std::uint64_t>(sinfo.psi.soi_so));
            if (socket_identity.second != 0U &&
                !observed_sockets.insert(socket_identity).second) {
                continue;
            }

            connection_common::connection_record rec;
            rec.pid = static_cast<std::uint32_t>(pid);
            rec.send_queue_bytes = static_cast<std::uint32_t>(sinfo.psi.soi_snd.sbi_cc);
            rec.receive_queue_bytes = static_cast<std::uint32_t>(sinfo.psi.soi_rcv.sbi_cc);

            if (kind == SOCKINFO_TCP && proto == IPPROTO_TCP) {
                rec.transport_protocol = connection_common::protocol::tcp;
                rec.state = macos_impl::map_macos_tcp_state(sinfo.psi.soi_proto.pri_tcp.tcpsi_state);

                if (family == AF_INET) {
                    const auto& in = sinfo.psi.soi_proto.pri_tcp.tcpsi_ini;
                    rec.local_endpoint.address = macos_impl::make_ipv4_address(
                        in.insi_laddr.ina_46.i46a_addr4.s_addr);
                    rec.local_endpoint.port = ntohs(static_cast<uint16_t>(in.insi_lport));

                    if (macos_impl::is_ipv4_zero(
                            in.insi_faddr.ina_46.i46a_addr4.s_addr) &&
                        in.insi_fport == 0) {
                        rec.remote_endpoint = std::nullopt;
                    } else {
                        connection_common::socket_endpoint_record remote;
                        remote.address = macos_impl::make_ipv4_address(
                            in.insi_faddr.ina_46.i46a_addr4.s_addr);
                        remote.port = ntohs(static_cast<uint16_t>(in.insi_fport));
                        rec.remote_endpoint = remote;
                    }
                } else {
                    const auto& in = sinfo.psi.soi_proto.pri_tcp.tcpsi_ini;
                    const std::uint32_t scope_id =
                        static_cast<std::uint32_t>(in.insi_v6.in6_ifindex);
                    rec.local_endpoint.address = macos_impl::make_ipv6_address(
                        in.insi_laddr.ina_6, scope_id);
                    rec.local_endpoint.port = ntohs(static_cast<uint16_t>(in.insi_lport));

                    if (macos_impl::is_ipv6_zero(in.insi_faddr.ina_6) && in.insi_fport == 0) {
                        rec.remote_endpoint = std::nullopt;
                    } else {
                        connection_common::socket_endpoint_record remote;
                        remote.address = macos_impl::make_ipv6_address(
                            in.insi_faddr.ina_6, scope_id);
                        remote.port = ntohs(static_cast<uint16_t>(in.insi_fport));
                        rec.remote_endpoint = remote;
                    }
                }
                records.push_back(std::move(rec));
            } else if (kind == SOCKINFO_IN && proto == IPPROTO_UDP) {
                rec.transport_protocol = connection_common::protocol::udp;
                rec.state = connection_common::tcp_state::unknown;

                if (family == AF_INET) {
                    const auto& in = sinfo.psi.soi_proto.pri_in;
                    rec.local_endpoint.address = macos_impl::make_ipv4_address(
                        in.insi_laddr.ina_46.i46a_addr4.s_addr);
                    rec.local_endpoint.port = ntohs(static_cast<uint16_t>(in.insi_lport));

                    if (macos_impl::is_ipv4_zero(
                            in.insi_faddr.ina_46.i46a_addr4.s_addr) &&
                        in.insi_fport == 0) {
                        rec.remote_endpoint = std::nullopt;
                    } else {
                        connection_common::socket_endpoint_record remote;
                        remote.address = macos_impl::make_ipv4_address(
                            in.insi_faddr.ina_46.i46a_addr4.s_addr);
                        remote.port = ntohs(static_cast<uint16_t>(in.insi_fport));
                        rec.remote_endpoint = remote;
                    }
                } else {
                    const auto& in = sinfo.psi.soi_proto.pri_in;
                    const std::uint32_t scope_id =
                        static_cast<std::uint32_t>(in.insi_v6.in6_ifindex);
                    rec.local_endpoint.address = macos_impl::make_ipv6_address(
                        in.insi_laddr.ina_6, scope_id);
                    rec.local_endpoint.port = ntohs(static_cast<uint16_t>(in.insi_lport));

                    if (macos_impl::is_ipv6_zero(in.insi_faddr.ina_6) && in.insi_fport == 0) {
                        rec.remote_endpoint = std::nullopt;
                    } else {
                        connection_common::socket_endpoint_record remote;
                        remote.address = macos_impl::make_ipv6_address(
                            in.insi_faddr.ina_6, scope_id);
                        remote.port = ntohs(static_cast<uint16_t>(in.insi_fport));
                        rec.remote_endpoint = remote;
                    }
                }
                records.push_back(std::move(rec));
            }
        }
    }

    connection_common::sort_connection_records(records);
    return records;
}

} // namespace connection_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_CONNECTION_MACOS_HPP
