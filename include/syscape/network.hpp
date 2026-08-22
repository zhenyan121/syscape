#ifndef SYSCAPE_NETWORK_HPP
#define SYSCAPE_NETWORK_HPP

/// @file
/// @brief Hosted network interface and unicast-address queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux and macOS enumerate interfaces through the documented
/// getifaddrs interface, resolving interface indices through POSIX
/// if_nametoindex; Linux exposes link-layer addresses through AF_PACKET
/// rows and macOS through AF_LINK rows. Windows enumerates adapters through
/// GetAdaptersAddresses and requires Windows Vista or later. Applications
/// that use this header on Windows must link the Iphlpapi import library;
/// Syscape itself stays header-only and does not add linkage for unrelated
/// Hosted Full domains. Other targets use the generic not-supported fallback.
/// @note This first network slice exposes interface names, indices,
/// operational state, loopback classification, link-layer (hardware)
/// addresses, and unicast IPv4/IPv6 addresses with prefix lengths. MTU,
/// routes, gateways, DNS configuration, host and domain names, and IPv6
/// zone identifiers are outside this slice. Expected failures are returned
/// as native error codes where available, or as syscape::errc values for
/// missing, malformed, or unsupported data.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/network.hpp requires C++17 or later"
#endif

#include <array>
#include <cstdint>
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
/// @return A list with at least one entry on any running hosted system,
/// invalid_encoding when a name is not valid UTF-8, malformed_data for
/// unusable platform records, not_supported when the platform exposes no
/// acceptable source or cannot represent a record, or a native platform
/// error.
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
            entry.addresses.push_back(std::move(converted));
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

} // namespace network
} // namespace syscape

#endif
