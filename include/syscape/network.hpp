#ifndef SYSCAPE_NETWORK_HPP
#define SYSCAPE_NETWORK_HPP

/// @file
/// @brief Hosted network interface, address, route, and gateway queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux and macOS enumerate interfaces through the documented
/// getifaddrs interface, resolving interface indices through POSIX
/// if_nametoindex; Linux exposes link-layer addresses through AF_PACKET
/// rows and macOS through AF_LINK rows. Windows enumerates adapters through
/// GetAdaptersAddresses. Linux obtains routes from NETLINK_ROUTE, Windows
/// from GetIpForwardTable2 with GetUnicastIpAddressTable context, and macOS
/// from a PF_ROUTE NET_RT_DUMP2 sysctl.
/// The Windows sources require Windows Vista or later. Applications that use
/// this header on Windows must link the Iphlpapi import library;
/// Syscape itself stays header-only and does not add linkage for unrelated
/// Hosted Full domains. Other targets use the generic not-supported fallback.
/// @note The implemented network slices expose interface names, indices,
/// operational state, loopback classification, link-layer (hardware)
/// addresses, MTU values, and unicast IPv4/IPv6 addresses with prefix lengths
/// and numeric IPv6 scope identifiers, together with forwarding unicast
/// routes and explicit default gateways. DNS configuration and host and
/// domain names are outside these slices. Expected failures are
/// returned as native error codes where available, or as syscape::errc values for
/// missing, malformed, or unsupported data.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/network.hpp requires C++17 or later"
#endif

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <syscape/detail/network/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/network/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/network/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/network/macos.hpp>
#else
#include <syscape/detail/network/generic.hpp>
#endif

namespace syscape {
namespace network {

/// Address family of one unicast network address.
using address_family = detail::network_common::address_family;

/// Operational state of one network interface as reported by the platform.
using interface_state = detail::network_common::interface_state;

/// One unicast address assigned to a network interface.
struct unicast_address {
    /// Family selects how many leading bytes of value are meaningful.
    address_family family;
    /// Address octets in network byte order, which is also their
    /// conventional presentation order. An IPv4 address occupies the first
    /// four bytes and the remaining bytes are zero.
    std::array<unsigned char, 16> value;
    /// Prefix length in bits: at most 32 for IPv4 and 128 for IPv6. The
    /// prefix describes the subnet mask recorded for the address at the
    /// moment of the query.
    std::uint8_t prefix_length;
    /// Numeric IPv6 scope identifier recorded by the platform. For a
    /// link-local address this normally identifies the interface on which
    /// the address is meaningful. IPv4 addresses always use zero, and zero
    /// is valid for an IPv6 address with no recorded zone.
    std::uint32_t scope_id;
};

/// One network interface with its assigned unicast addresses.
///
/// Every field reports the state observed during enumeration; concurrent
/// reconfiguration becomes visible only to later calls.
struct interface_entry {
    /// Operating-system interface name, reported verbatim without
    /// canonicalization. The name can refer to an interface that was
    /// removed after the snapshot was taken.
    std::string name;
    /// Interface index assigned by the operating system. On Windows the IPv4
    /// index is preferred when available and the IPv6 index is used for an
    /// IPv6-only adapter. The index is always nonzero because every platform
    /// that exposes indices reserves zero. Indices can be reassigned after
    /// interfaces are removed.
    std::uint32_t index;
    /// Operational state observed during enumeration. An administratively
    /// up interface whose lower layer is not passing traffic is reported as
    /// unknown rather than forced into either up or down.
    interface_state state;
    /// True when the interface is a loopback interface.
    bool loopback;
    /// Link-layer (hardware) address bytes verbatim from the platform, in
    /// transmission order. Ethernet and wireless adapters report six bytes.
    /// An empty vector is valid data meaning that the platform records no
    /// hardware address for this interface, which is typical for loopback
    /// and tunnel interfaces.
    std::vector<std::uint8_t> hardware_address;
    /// Unicast addresses assigned to the interface in platform enumeration
    /// order. An empty vector is valid data for an interface with no
    /// assigned unicast addresses.
    std::vector<unicast_address> addresses;
    /// Maximum transmission unit recorded for this interface, in bytes.
    /// The value can change when the interface is reconfigured and is always
    /// nonzero in a successful snapshot.
    std::uint32_t mtu_bytes;
};

/// One IPv4 or IPv6 address used by a route.
struct ip_address {
    /// Address family selecting the meaningful length of value.
    address_family family;
    /// Address octets in network byte order. IPv4 uses the first four bytes.
    std::array<unsigned char, 16> value;
    /// Numeric IPv6 scope identifier. IPv4 always uses zero.
    std::uint32_t scope_id;
};

/// One forwarding unicast route from the platform routing table.
struct route_entry {
    /// Canonical destination network address with all host bits clear.
    ip_address destination;
    /// Destination prefix length in bits.
    std::uint8_t prefix_length;
    /// Explicit next hop, or no value for an on-link route.
    std::optional<ip_address> next_hop;
    /// Nonzero operating-system interface index used by the route.
    std::uint32_t interface_index;
    /// Platform-recorded route metric, when the source exposes one. Values
    /// from different operating systems are not directly comparable.
    std::optional<std::uint32_t> metric;
};

/// One explicit gateway selected by a default route.
struct gateway_entry {
    /// Explicit next-hop address recorded by the default route.
    ip_address address;
    /// Nonzero operating-system interface index used by the route.
    std::uint32_t interface_index;
    /// Platform-recorded route metric, when available.
    std::optional<std::uint32_t> metric;
};

/// Returns a snapshot of the platform's network interfaces and their
/// unicast addresses.
///
/// The snapshot reflects the interfaces observed during the call; concurrent
/// configuration changes become visible only to later calls. Interfaces
/// that expose no unicast addresses are still listed. Address families that
/// this slice does not represent are skipped without failing the query.
///
/// On Linux, a recorded link-layer address longer than the eight bytes the
/// documented AF_PACKET socket-address storage holds cannot be represented
/// by this source; such a record fails the whole snapshot with
/// not_supported rather than being truncated.
///
/// On Windows, an IPv6-only adapter uses Ipv6IfIndex when its documented
/// IPv4 IfIndex is zero. An adapter for which both protocol indices are zero
/// cannot satisfy the portable nonzero-index contract; such a record fails
/// the whole snapshot with not_supported rather than fabricating an index.
///
/// Linux and macOS obtain the MTU by name after enumerating the interface
/// table. If an interface disappears or is renamed during that interval,
/// the native lookup failure is returned rather than publishing a partial
/// snapshot.
///
/// @return A list with at least one entry on any running hosted system,
/// invalid_encoding when a name is not valid UTF-8, malformed_data for
/// unusable platform records including a zero MTU or nonzero IPv4 scope
/// identifier, not_supported when the platform exposes no acceptable source
/// or cannot represent a record, or a native platform error.
inline result<std::vector<interface_entry>> interfaces() {
    result<std::vector<detail::network_common::interface_record>> records =
        detail::network_common::validate_interface_records(
            detail::network_backend::interfaces());
    if (!records) { return fail(records.error()); }
    std::vector<interface_entry> entries;
    entries.reserve(records->size());
    for (detail::network_common::interface_record& record : *records) {
        interface_entry entry;
        entry.name = std::move(record.name);
        entry.index = record.index;
        entry.state = record.state;
        entry.loopback = record.loopback;
        entry.hardware_address = std::move(record.hardware_address);
        entry.addresses.reserve(record.addresses.size());
        for (detail::network_common::unicast_record& address :
             record.addresses) {
            unicast_address converted;
            converted.family = address.family;
            converted.value = address.value;
            converted.prefix_length = address.prefix_length;
            converted.scope_id = address.scope_id;
            entry.addresses.push_back(std::move(converted));
        }
        entry.mtu_bytes = record.mtu_bytes;
        entries.push_back(std::move(entry));
    }
    return entries;
}

/// Returns the platform's forwarding IPv4 and IPv6 unicast routes.
///
/// Non-forwarding entries such as local, broadcast, blackhole, prohibit, and
/// unreachable routes are omitted. Enumeration order is preserved. An empty
/// snapshot is valid. Concurrent route changes are visible to later calls.
///
/// @return The route snapshot, malformed_data for structurally unusable
/// platform records, not_supported where no documented source exists or a
/// platform route representation cannot be resolved portably, or a native
/// platform error.
inline result<std::vector<route_entry>> routes() {
    result<std::vector<detail::network_common::route_record>> records =
        detail::network_common::validate_route_records(
            detail::network_backend::routes());
    if (!records) { return fail(records.error()); }
    std::vector<route_entry> entries;
    entries.reserve(records->size());
    for (detail::network_common::route_record& record : *records) {
        route_entry entry;
        entry.destination = ip_address {record.destination.family,
                                        record.destination.value,
                                        record.destination.scope_id};
        entry.prefix_length = record.prefix_length;
        if (record.next_hop) {
            entry.next_hop = ip_address {record.next_hop->family,
                                         record.next_hop->value,
                                         record.next_hop->scope_id};
        }
        entry.interface_index = record.interface_index;
        entry.metric = record.metric;
        entries.push_back(std::move(entry));
    }
    return entries;
}

/// Returns explicit gateways carried by the platform's default routes.
///
/// An on-link default route has no gateway and is intentionally omitted.
/// Multiple default gateways and their platform enumeration order are
/// preserved. No explicit default gateway is a successful empty result.
///
/// @return Explicit next hops from the route snapshot, or the same error
/// that routes() would return for the current platform state.
inline result<std::vector<gateway_entry>> default_gateways() {
    const result<std::vector<route_entry>> listed = routes();
    if (!listed) { return fail(listed.error()); }
    std::vector<gateway_entry> gateways;
    for (const route_entry& route : *listed) {
        if (route.prefix_length == 0U && route.next_hop) {
            gateways.push_back(gateway_entry {*route.next_hop,
                                               route.interface_index,
                                               route.metric});
        }
    }
    return gateways;
}

} // namespace network
} // namespace syscape

#endif
