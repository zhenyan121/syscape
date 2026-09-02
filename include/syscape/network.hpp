#ifndef SYSCAPE_NETWORK_HPP
#define SYSCAPE_NETWORK_HPP

/// @file
/// @brief Hosted network interface, address, route, gateway, and statistics
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms and Android).
/// @note Linux, macOS, Apple mobile platforms (iOS, iPadOS, tvOS, watchOS,
/// visionOS, and Mac Catalyst), and FreeBSD enumerate interfaces through the
/// documented getifaddrs interface, resolving interface indices through POSIX
/// if_nametoindex; Linux exposes link-layer addresses through AF_PACKET
/// rows and macOS/Apple mobile/FreeBSD through AF_LINK rows. Windows enumerates
/// adapters through GetAdaptersAddresses. Linux obtains routes from
/// NETLINK_ROUTE, Windows from GetIpForwardTable2 with
/// GetUnicastIpAddressTable context, and macOS from a PF_ROUTE NET_RT_DUMP2
/// sysctl. Interface traffic and error
/// statistics query /proc/net/dev and sysfs on Linux, GetIfTable2 / GetIfEntry2
/// on Windows, and a PF_ROUTE NET_RT_IFLIST2 sysctl on macOS. Apple mobile
/// public SDKs do not expose the routing definitions required by those macOS
/// sources, so route, gateway, DNS, and interface-statistics queries report
/// not_supported there.
/// The Windows sources require Windows Vista or later. Applications that use
/// this header on Windows must link the Iphlpapi import library;
/// applications using this header on Solaris link -lsocket -lnsl;
/// Syscape itself stays header-only and does not add linkage for unrelated
/// Hosted Full domains. FreeBSD reads resolv.conf and getifaddrs traffic
/// statistics, and reports routes and gateways as unsupported. Other targets
/// use the generic not-supported fallback.
/// @note Android interface enumeration requires API level 24 or later and
/// reports not_supported on earlier API levels. Opening the AF_INET socket
/// used for MTU queries may require android.permission.INTERNET.
/// @note The implemented network slices expose interface names, indices,
/// operational state, loopback classification, link-layer (hardware)
/// addresses, MTU values, and unicast IPv4/IPv6 addresses with prefix lengths
/// and numeric IPv6 scope identifiers, forwarding unicast routes, explicit
/// default gateways, the system DNS resolver configuration, and cumulative
/// interface traffic and error statistics (rx/tx bytes, packets, errors, drops,
/// multicast, and collisions). Host-name queries are provided by syscape/os.hpp
/// rather than duplicated here. Expected failures are returned as native error
/// codes where available, or as syscape::errc values for missing, malformed, or
/// unsupported data. On macOS this header also requires linking the
/// SystemConfiguration and CoreFoundation frameworks; on Windows it requires
/// the Iphlpapi import library documented below.

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
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/network/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/network/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/network/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/network/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/network/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/network/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/network/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/network/solaris.hpp>
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
    /// The value can change when the interface is reconfigured. Zero is a
    /// valid native value for an interface that does not transmit packets
    /// directly, such as an OpenBSD IPsec filtering interface.
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
/// On Android, opening a socket for MTU ioctl resolution requires the
/// application to hold android.permission.INTERNET; queries without that
/// permission report permission_denied or operation_not_permitted.
///
/// @return A list with at least one entry on any running hosted system,
/// invalid_encoding when a name is not valid UTF-8, malformed_data for
/// unusable platform records including a nonzero IPv4 scope identifier,
/// not_supported when the platform exposes no acceptable source or cannot
/// represent a record, or a native platform error.
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

/// One configured DNS resolver server address.
struct dns_server_entry {
    /// Resolver address recorded by the platform. An IPv6 link-local
    /// resolver carries its numeric zone in the scope identifier.
    ip_address address;
    /// Interface the platform binds this resolver to, when the source
    /// records a binding. No value means that the source binds the
    /// resolver to no particular interface. A set binding is nonzero and
    /// can be reassigned after interfaces are removed.
    std::optional<std::uint32_t> interface_index;
};

/// One snapshot of the platform's DNS resolver configuration.
///
/// The server collection is valid while empty. A present search-domain list
/// is likewise valid while empty; an absent list means that the backend's
/// documented sources cannot expose that field. The snapshot reflects the
/// configuration observed during the call; concurrent reconfiguration
/// becomes visible only to later calls.
struct dns_configuration {
    /// Resolver addresses in resolution-attempt order as recorded by the
    /// platform, duplicates preserved verbatim when several sources list
    /// the same resolver.
    std::vector<dns_server_entry> servers;
    /// Ordered global search domains when the platform source exposes that
    /// list. A present empty vector means that it records no search list;
    /// no value means that no documented source used by this backend can
    /// expose the field.
    std::optional<std::vector<std::string>> search_domains;
    /// Local domain name recorded separately by the platform source, when
    /// it exposes one. No value means that the source records no distinct
    /// local domain; an empty string is never reported instead.
    std::optional<std::string> domain_name;
};

/// Returns a snapshot of the platform's DNS resolver configuration.
///
/// The query performs no network requests; it reads only local
/// configuration records.
///
/// On Linux the snapshot parses /etc/resolv.conf, the documented resolver
/// file: nameserver directives supply resolver addresses, the last
/// instance of the mutually exclusive search or domain directives
/// determines both the search list and the local domain, and options,
/// sortlist, and unrecognized directives are skipped exactly as the
/// documented consumer ignores them. Environment overrides such as
/// LOCALDOMAIN and RES_OPTIONS, nsswitch policy, and dynamically managed
/// stub-resolver arrangements are outside this verbatim file view. An
/// absent file reports not_found because the platform then records no
/// DNS configuration.
///
/// On Windows resolver servers come from the documented per-adapter
/// GetAdaptersAddresses chains concatenated in adapter enumeration order
/// and preserved within each adapter, including duplicates across adapters,
/// each bound to its adapter's interface index; the local domain name comes
/// from the documented global GetNetworkParams DomainName field. These APIs
/// do not expose the distinct global suffix search list, so search_domains
/// has no value. The per-adapter binding means two entries can carry one
/// address with different bindings. The Windows resolver merges these
/// records by its own rules during resolution, which this snapshot does not
/// reproduce.
///
/// On macOS the snapshot reads the State:/Network/Global/DNS entity of
/// the documented SystemConfiguration dynamic store; its keys are absent
/// on a system that records no global DNS configuration, which reports
/// not_found. Resolvers reported there carry no interface binding.
///
/// @return The configuration snapshot, invalid_encoding for text that is
/// not valid UTF-8, malformed_data for unusable platform records,
/// not_found where the platform records no configuration at all,
/// not_supported where no documented source exists, or a native platform
/// error.
inline result<dns_configuration> dns() {
    const result<detail::network_common::dns_record> validated =
        detail::network_common::validate_dns_record(
            detail::network_backend::dns());
    if (!validated) { return fail(validated.error()); }
    dns_configuration configuration;
    configuration.servers.reserve(validated->servers.size());
    for (const detail::network_common::dns_server_record& server :
         validated->servers) {
        configuration.servers.push_back(dns_server_entry {
            ip_address {server.address.family,
                        server.address.value,
                        server.address.scope_id},
            server.interface_index});
    }
    configuration.search_domains = std::move(validated->search_domains);
    configuration.domain_name = std::move(validated->domain_name);
    return configuration;
}

/// Cumulative network interface traffic, packet, and error metrics.
struct interface_statistics {
    /// Operating-system interface name, reported verbatim without
    /// canonicalization.
    std::string name;

    /// Nonzero operating-system interface index.
    std::uint32_t index = 0U;

    /// Cumulative bytes received by this interface.
    std::uint64_t rx_bytes = 0U;

    /// Cumulative bytes transmitted by this interface.
    std::uint64_t tx_bytes = 0U;

    /// Cumulative packets received by this interface.
    std::uint64_t rx_packets = 0U;

    /// Cumulative packets transmitted by this interface.
    std::uint64_t tx_packets = 0U;

    /// Cumulative receive errors reported for this interface.
    std::uint64_t rx_errors = 0U;

    /// Cumulative transmit errors reported for this interface.
    std::uint64_t tx_errors = 0U;

    /// Cumulative incoming packets dropped by this interface.
    std::uint64_t rx_dropped = 0U;

    /// Cumulative outgoing packets dropped by this interface.
    std::uint64_t tx_dropped = 0U;

    /// Multicast packets received, when reported by the platform.
    std::optional<std::uint64_t> rx_multicast;

    /// Transmit collisions detected, when reported by the platform.
    std::optional<std::uint64_t> collisions;
};

} // namespace network

namespace detail {
namespace network_public {

inline network::interface_statistics make_public_statistics(
    network_common::statistics_record&& rec) {
    network::interface_statistics entry;
    entry.name = std::move(rec.name);
    entry.index = rec.index;
    entry.rx_bytes = rec.rx_bytes;
    entry.tx_bytes = rec.tx_bytes;
    entry.rx_packets = rec.rx_packets;
    entry.tx_packets = rec.tx_packets;
    entry.rx_errors = rec.rx_errors;
    entry.tx_errors = rec.tx_errors;
    entry.rx_dropped = rec.rx_dropped;
    entry.tx_dropped = rec.tx_dropped;
    entry.rx_multicast = rec.rx_multicast;
    entry.collisions = rec.collisions;
    return entry;
}

} // namespace network_public
} // namespace detail

namespace network {

/// Returns traffic and error statistics for all observable network interfaces.
///
/// The snapshot reflects the counters observed during the call; values change
/// continuously with system network activity. The counters are cumulative since
/// interface creation or system boot and can wrap when a platform's native
/// counter representation reaches its limit.
///
/// @return A snapshot of interface statistics, malformed_data for unusable
/// platform records, invalid_encoding when a name is not valid UTF-8,
/// not_supported when the platform exposes no acceptable source, or a native
/// platform error.
inline result<std::vector<interface_statistics>> statistics() {
    result<std::vector<detail::network_common::statistics_record>> records =
        detail::network_common::validate_statistics_records(
            detail::network_backend::statistics());
    if (!records) { return fail(records.error()); }
    std::vector<interface_statistics> entries;
    entries.reserve(records->size());
    for (detail::network_common::statistics_record& record : *records) {
        entries.push_back(
            detail::network_public::make_public_statistics(std::move(record)));
    }
    return entries;
}

/// Returns traffic and error statistics for a specific interface by name.
///
/// @param interface_name Exact operating-system interface name (UTF-8).
/// @return The interface's statistics, not_found if no interface with that name
/// exists, invalid_argument if the name is empty or contains embedded nulls,
/// invalid_encoding if the name is not valid UTF-8, not_supported when
/// the platform exposes no acceptable source, or a native platform error.
inline result<interface_statistics> statistics(
    std::string_view interface_name) {
    if (interface_name.empty() ||
        interface_name.find('\0') != std::string_view::npos) {
        return fail(errc::invalid_argument);
    }
    if (!detail::is_valid_utf8(interface_name)) {
        return fail(errc::invalid_encoding);
    }
    result<detail::network_common::statistics_record> record =
        detail::network_common::validate_statistics_record(
            detail::network_backend::statistics_by_name(interface_name));
    if (!record) { return fail(record.error()); }
    return detail::network_public::make_public_statistics(std::move(*record));
}

/// Returns traffic and error statistics for a specific interface by OS index.
///
/// @param interface_index Nonzero operating-system interface index.
/// @return The interface's statistics, not_found if no interface with that
/// index exists, invalid_argument if index is 0, not_supported when the
/// platform exposes no acceptable source, or a native platform error.
inline result<interface_statistics> statistics(std::uint32_t interface_index) {
    if (interface_index == 0U) {
        return fail(errc::invalid_argument);
    }
    result<detail::network_common::statistics_record> record =
        detail::network_common::validate_statistics_record(
            detail::network_backend::statistics_by_index(interface_index));
    if (!record) { return fail(record.error()); }
    return detail::network_public::make_public_statistics(std::move(*record));
}

} // namespace network
} // namespace syscape

#endif
