#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <syscape/connection.hpp>
#include <syscape/network.hpp>
#include <syscape/wifi.hpp>

namespace {

std::string format_ip(const syscape::network::ip_address& ip) {
    std::ostringstream ss;
    if (ip.family == syscape::network::address_family::ipv4) {
        ss << static_cast<int>(ip.value[0]) << "."
           << static_cast<int>(ip.value[1]) << "."
           << static_cast<int>(ip.value[2]) << "."
           << static_cast<int>(ip.value[3]);
    } else {
        for (std::size_t i = 0; i < 16; i += 2) {
            if (i > 0) { ss << ":"; }
            const std::uint16_t word = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(ip.value[i]) << 8) |
                static_cast<std::uint16_t>(ip.value[i + 1]));
            ss << std::hex << word << std::dec;
        }
    }
    return ss.str();
}

std::string format_conn_ip(const syscape::connection::ip_address& ip) {
    std::ostringstream ss;
    if (ip.family == syscape::connection::address_family::ipv4) {
        ss << static_cast<int>(ip.value[0]) << "."
           << static_cast<int>(ip.value[1]) << "."
           << static_cast<int>(ip.value[2]) << "."
           << static_cast<int>(ip.value[3]);
    } else {
        for (std::size_t i = 0; i < 16; i += 2) {
            if (i > 0) { ss << ":"; }
            const std::uint16_t word = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(ip.value[i]) << 8) |
                static_cast<std::uint16_t>(ip.value[i + 1]));
            ss << std::hex << word << std::dec;
        }
    }
    return ss.str();
}

std::string format_mac(const std::vector<std::uint8_t>& mac) {
    std::ostringstream ss;
    for (std::size_t i = 0; i < mac.size(); ++i) {
        if (i > 0) { ss << ":"; }
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(mac[i]);
    }
    return ss.str();
}

const char* interface_state_name(syscape::network::interface_state state) {
    switch (state) {
    case syscape::network::interface_state::up: return "UP";
    case syscape::network::interface_state::down: return "DOWN";
    case syscape::network::interface_state::unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* tcp_state_name(syscape::connection::tcp_state state) {
    switch (state) {
    case syscape::connection::tcp_state::established: return "ESTABLISHED";
    case syscape::connection::tcp_state::listen: return "LISTEN";
    case syscape::connection::tcp_state::syn_sent: return "SYN_SENT";
    case syscape::connection::tcp_state::syn_recv: return "SYN_RECV";
    case syscape::connection::tcp_state::fin_wait1: return "FIN_WAIT1";
    case syscape::connection::tcp_state::fin_wait2: return "FIN_WAIT2";
    case syscape::connection::tcp_state::time_wait: return "TIME_WAIT";
    case syscape::connection::tcp_state::closed: return "CLOSED";
    case syscape::connection::tcp_state::close_wait: return "CLOSE_WAIT";
    case syscape::connection::tcp_state::last_ack: return "LAST_ACK";
    case syscape::connection::tcp_state::closing: return "CLOSING";
    case syscape::connection::tcp_state::unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

} // namespace

int main() {
    std::cout << "=== Syscape Network & Wireless Overview Example ===" << std::endl;

    // Network Interfaces
    std::cout << "\n[Network Interfaces]" << std::endl;
    if (const auto ifaces = syscape::network::interfaces()) {
        for (const auto& iface : *ifaces) {
            std::cout << "  Interface: " << iface.name
                      << " (Index: " << iface.index << ")" << std::endl;
            std::cout << "    State:    "
                      << interface_state_name(iface.state)
                      << (iface.loopback ? " [Loopback]" : "") << std::endl;
            if (!iface.hardware_address.empty()) {
                std::cout << "    MAC:      " << format_mac(iface.hardware_address) << std::endl;
            }
            if (iface.mtu_bytes > 0) {
                std::cout << "    MTU:      " << iface.mtu_bytes << " bytes" << std::endl;
            }
            for (const auto& addr : iface.addresses) {
                const syscape::network::ip_address ip_addr{addr.family, addr.value, addr.scope_id};
                std::cout << "    IP:       " << format_ip(ip_addr) << "/"
                          << static_cast<int>(addr.prefix_length) << std::endl;
            }
        }
    }

    // Default Gateways
    std::cout << "\n[Default Gateways]" << std::endl;
    if (const auto gateways = syscape::network::default_gateways()) {
        for (const auto& gw : *gateways) {
            std::cout << "  Gateway:       " << format_ip(gw.address)
                      << " (Interface Index: " << gw.interface_index << ")"
                      << std::endl;
        }
    }

    // DNS Configuration
    std::cout << "\n[DNS Resolver Configuration]" << std::endl;
    if (const auto dns_cfg = syscape::network::dns()) {
        for (const auto& server : dns_cfg->servers) {
            std::cout << "  Nameserver:    " << format_ip(server.address) << std::endl;
        }
        if (dns_cfg->search_domains) {
            for (const auto& domain : *dns_cfg->search_domains) {
                std::cout << "  Search Domain: " << domain << std::endl;
            }
        }
        if (dns_cfg->domain_name) {
            std::cout << "  Domain Name:   " << *dns_cfg->domain_name << std::endl;
        }
    }

    // Wi-Fi Status
    std::cout << "\n[Wireless (Wi-Fi) Adapters & Connection]" << std::endl;
    if (const auto wifi_adapters = syscape::wifi::adapters()) {
        for (const auto& ad : *wifi_adapters) {
            std::cout << "  Wi-Fi Adapter [" << ad.id << "]: " << ad.name << std::endl;
            if (ad.mac_address) {
                std::cout << "    MAC:         " << *ad.mac_address << std::endl;
            }
        }
    }
    if (const auto wifi_conn = syscape::wifi::current_connection(); wifi_conn && *wifi_conn) {
        const auto& conn = **wifi_conn;
        std::cout << "  Active SSID:   " << conn.ssid << " (" << conn.bssid << ")" << std::endl;
        if (conn.signal_dbm) {
            std::cout << "  Signal:        " << *conn.signal_dbm << " dBm";
            if (conn.signal_quality_percent) {
                std::cout << " (" << static_cast<int>(*conn.signal_quality_percent) << "% quality)";
            }
            std::cout << std::endl;
        }
        if (conn.frequency_mhz) {
            std::cout << "  Frequency:     " << *conn.frequency_mhz << " MHz";
            if (conn.channel) {
                std::cout << " (Channel " << *conn.channel << ")";
            }
            std::cout << std::endl;
        }
    }

    // Active Sockets & Connections
    std::cout << "\n[Active Network Sockets & Connections]" << std::endl;
    if (const auto conns = syscape::connection::connections()) {
        std::cout << "  Total Active Sockets: " << conns->size() << std::endl;
        for (std::size_t i = 0; i < std::min<std::size_t>(conns->size(), 6); ++i) {
            const auto& conn = (*conns)[i];
            std::cout << "    ["
                      << (conn.transport_protocol == syscape::connection::protocol::tcp ? "TCP" : "UDP")
                      << "] " << format_conn_ip(conn.local_endpoint.address)
                      << ":" << conn.local_endpoint.port;
            if (conn.remote_endpoint) {
                std::cout << " -> " << format_conn_ip(conn.remote_endpoint->address)
                          << ":" << conn.remote_endpoint->port;
            }
            std::cout << " [" << tcp_state_name(conn.state) << "]";
            if (conn.pid) {
                std::cout << " (PID " << *conn.pid << ")";
            }
            std::cout << std::endl;
        }
        if (conns->size() > 6) {
            std::cout << "    ... and " << (conns->size() - 6) << " more active sockets." << std::endl;
        }
    }

    // Interface Statistics
    std::cout << "\n[Interface Traffic & Statistics]" << std::endl;
    if (const auto stats = syscape::network::statistics()) {
        for (const auto& stat : *stats) {
            std::cout << "  Stats for " << stat.name << ":" << std::endl;
            std::cout << "    RX: " << (stat.rx_bytes / (1024ULL * 1024ULL))
                      << " MiB (" << stat.rx_packets << " packets, "
                      << stat.rx_errors << " errors, "
                      << stat.rx_dropped << " drops)" << std::endl;
            std::cout << "    TX: " << (stat.tx_bytes / (1024ULL * 1024ULL))
                      << " MiB (" << stat.tx_packets << " packets, "
                      << stat.tx_errors << " errors, "
                      << stat.tx_dropped << " drops)" << std::endl;
        }
    }

    return 0;
}
