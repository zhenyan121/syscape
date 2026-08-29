#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <syscape/network.hpp>

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

} // namespace

int main() {
    std::cout << "=== Syscape Network Overview Example ===" << std::endl;

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
    } else {
        std::cout << "  (Unable to query interfaces: "
                  << ifaces.error().message() << ")" << std::endl;
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
    } else {
        std::cout << "  (Unable to query DNS config: "
                  << dns_cfg.error().message() << ")" << std::endl;
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
