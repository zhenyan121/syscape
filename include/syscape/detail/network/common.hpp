#ifndef SYSCAPE_DETAIL_NETWORK_COMMON_HPP
#define SYSCAPE_DETAIL_NETWORK_COMMON_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_common {

/// Address family of one unicast network address.
enum class address_family : std::uint8_t { ipv4, ipv6 };

/// Operational state of one network interface as reported by the platform.
enum class interface_state : std::uint8_t {
    /// The platform reports neither an operative nor a non-operative
    /// condition, or reports a condition this slice does not represent.
    unknown,
    /// The interface is present and not operational.
    down,
    /// The interface is present and operational.
    up
};

/// Returns the maximum prefix length in bits for an address family.
inline std::uint8_t maximum_prefix_length(address_family family) noexcept {
    return family == address_family::ipv4 ? 32U : 128U;
}

/// One unicast address assigned to an interface, shared by the network
/// backends awaiting boundary validation.
struct unicast_record {
    /// Family selects how many leading bytes of value are meaningful.
    address_family family = address_family::ipv4;
    /// Address bytes in network byte order. IPv4 occupies the first four
    /// bytes and the remaining bytes are zero.
    std::array<unsigned char, 16> value {};
    /// Prefix length in bits, never greater than maximum_prefix_length().
    std::uint8_t prefix_length = 0U;
    /// Numeric IPv6 scope identifier recorded by the platform. IPv4 records
    /// always use zero; zero is also valid for an IPv6 address with no
    /// recorded zone.
    std::uint32_t scope_id = 0U;
};

/// One network interface with its addresses, shared by the network backends
/// awaiting boundary validation.
struct interface_record {
    /// Operating-system interface name. Never empty after validation.
    std::string name;
    /// Interface index assigned by the operating system; always nonzero
    /// because index zero is reserved by every platform that exposes
    /// indices.
    std::uint32_t index = 0U;
    /// Operational state observed during enumeration.
    interface_state state = interface_state::unknown;
    /// True when the interface is a loopback interface.
    bool loopback = false;
    /// Link-layer (hardware) address bytes verbatim from the platform, in
    /// transmission order. An empty vector is valid data meaning that the
    /// platform records no hardware address for this interface.
    std::vector<std::uint8_t> hardware_address;
    /// Unicast addresses assigned to the interface. An empty vector is
    /// valid data for an interface with no assigned unicast addresses.
    std::vector<unicast_record> addresses;
    /// Maximum transmission unit recorded for this interface, in bytes.
    /// Zero is not a usable MTU and is rejected at the public boundary.
    std::uint32_t mtu_bytes = 0U;
};

/// One IP address used at the routing boundary.
struct ip_address_record {
    address_family family = address_family::ipv4;
    std::array<unsigned char, 16> value {};
    std::uint32_t scope_id = 0U;
};

/// One forwarding unicast route reported by a platform backend.
struct route_record {
    ip_address_record destination;
    std::uint8_t prefix_length = 0U;
    std::optional<ip_address_record> next_hop;
    std::uint32_t interface_index = 0U;
    std::optional<std::uint32_t> metric;
};

inline result<void> validate_ip_address(const ip_address_record& address) {
    if (address.family != address_family::ipv4 &&
        address.family != address_family::ipv6) {
        return fail(errc::malformed_data);
    }
    if (address.family == address_family::ipv4) {
        if (address.scope_id != 0U) { return fail(errc::malformed_data); }
        for (std::size_t offset = 4U; offset < address.value.size(); ++offset) {
            if (address.value[offset] != 0U) {
                return fail(errc::malformed_data);
            }
        }
    }
    return {};
}

inline bool is_unspecified(const ip_address_record& address) noexcept {
    const std::size_t length =
        address.family == address_family::ipv4 ? 4U : 16U;
    for (std::size_t offset = 0U; offset < length; ++offset) {
        if (address.value[offset] != 0U) { return false; }
    }
    return true;
}

/// Validates route records without rewriting platform data.
inline result<std::vector<route_record>> validate_route_records(
    result<std::vector<route_record>> records) {
    if (!records) { return fail(records.error()); }
    for (const route_record& entry : *records) {
        const result<void> destination_valid =
            validate_ip_address(entry.destination);
        if (!destination_valid) { return fail(destination_valid.error()); }
        if (entry.interface_index == 0U ||
            entry.prefix_length > maximum_prefix_length(
                                      entry.destination.family)) {
            return fail(errc::malformed_data);
        }
        const std::size_t length =
            entry.destination.family == address_family::ipv4 ? 4U : 16U;
        const std::size_t whole =
            static_cast<std::size_t>(entry.prefix_length / 8U);
        const std::uint8_t remainder =
            static_cast<std::uint8_t>(entry.prefix_length % 8U);
        if (remainder != 0U && whole < length) {
            const unsigned char host_mask = static_cast<unsigned char>(
                (1U << (8U - remainder)) - 1U);
            if ((entry.destination.value[whole] & host_mask) != 0U) {
                return fail(errc::malformed_data);
            }
        }
        const std::size_t host_start = whole + (remainder == 0U ? 0U : 1U);
        for (std::size_t offset = host_start; offset < length; ++offset) {
            if (entry.destination.value[offset] != 0U) {
                return fail(errc::malformed_data);
            }
        }
        if (entry.next_hop) {
            const result<void> next_hop_valid =
                validate_ip_address(*entry.next_hop);
            if (!next_hop_valid) { return fail(next_hop_valid.error()); }
            if (entry.next_hop->family != entry.destination.family) {
                return fail(errc::malformed_data);
            }
        }
    }
    return records;
}

/// Validates converted interface records at the public boundary.
///
/// Every record needs a non-empty name in valid UTF-8, a nonzero index, and a
/// nonzero MTU. Each address needs a prefix length within its family's
/// documented range, and an IPv4 record must use a zero scope identifier and
/// leave the bytes beyond the first four zero. One unusable record fails the
/// whole snapshot so that silently dropping entries can never hide platform
/// damage.
inline result<std::vector<interface_record>> validate_interface_records(
    result<std::vector<interface_record>> records) {
    if (!records) { return fail(records.error()); }
    for (const interface_record& entry : *records) {
        if (entry.name.empty() || entry.index == 0U ||
            entry.mtu_bytes == 0U) {
            return fail(errc::malformed_data);
        }
        if (!is_valid_utf8(entry.name)) {
            return fail(errc::invalid_encoding);
        }
        for (const unicast_record& address : entry.addresses) {
            if (address.prefix_length >
                maximum_prefix_length(address.family)) {
                return fail(errc::malformed_data);
            }
            if (address.family == address_family::ipv4) {
                if (address.scope_id != 0U) {
                    return fail(errc::malformed_data);
                }
                for (std::size_t offset = 4U; offset < address.value.size();
                     ++offset) {
                    if (address.value[offset] != 0U) {
                        return fail(errc::malformed_data);
                    }
                }
            }
        }
    }
    return records;
}

} // namespace network_common
} // namespace detail
} // namespace syscape

#endif
