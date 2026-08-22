#ifndef SYSCAPE_DETAIL_NETWORK_COMMON_HPP
#define SYSCAPE_DETAIL_NETWORK_COMMON_HPP

#include <array>
#include <cstdint>
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
};

/// Validates converted interface records at the public boundary.
///
/// Every record needs a non-empty name in valid UTF-8 and a nonzero index.
/// Each address needs a prefix length within its family's documented range,
/// and an IPv4 record must leave the bytes beyond the first four zero. One
/// unusable record fails the whole snapshot so that silently dropping
/// entries can never hide platform damage.
inline result<std::vector<interface_record>> validate_interface_records(
    result<std::vector<interface_record>> records) {
    if (!records) { return fail(records.error()); }
    for (const interface_record& entry : *records) {
        if (entry.name.empty() || entry.index == 0U) {
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
