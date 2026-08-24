#ifndef SYSCAPE_DETAIL_HARDWARE_COMMON_HPP
#define SYSCAPE_DETAIL_HARDWARE_COMMON_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_common {

using chassis_classification = ::syscape::hardware::form_factor;

/// Validates one converted identity string at the public boundary.
///
/// Hosted Full text is UTF-8 by contract, so a backend rendering that does
/// not decode reports invalid_encoding instead of corrupted text. An empty
/// string can never satisfy an identity query because every backend records
/// absence as not_found rather than as empty text; reaching this boundary
/// with emptiness therefore means contradictory platform data.
inline result<std::string> validate_identity_text(
    result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty()) { return fail(errc::malformed_data); }
    if (!is_valid_utf8(*value)) { return fail(errc::invalid_encoding); }
    return value;
}

/// Maps one recorded SMBIOS System Enclosure Type byte onto the shared
/// form-factor vocabulary.
///
/// Both implemented classification sources publish this identical table: the
/// Linux DMI-id interface renders the recorded byte verbatim and the Windows
/// backend parses the same byte out of the raw SMBIOS enclosure record.
/// Renderings outside the documented one-based range are malformed platform
/// data because guessing a nearest value would fabricate structure the
/// firmware did not record.
inline result<chassis_classification> classify_chassis(
    std::uint8_t recorded) {
    switch (recorded) {
    case 1U: return chassis_classification::other;
    case 2U: return chassis_classification::unknown;
    case 3U: return chassis_classification::desktop;
    case 4U: return chassis_classification::low_profile_desktop;
    case 5U: return chassis_classification::pizza_box;
    case 6U: return chassis_classification::mini_tower;
    case 7U: return chassis_classification::tower;
    case 8U: return chassis_classification::portable;
    case 9U: return chassis_classification::laptop;
    case 10U: return chassis_classification::notebook;
    case 11U: return chassis_classification::hand_held;
    case 12U: return chassis_classification::docking_station;
    case 13U: return chassis_classification::all_in_one;
    case 14U: return chassis_classification::sub_notebook;
    case 15U: return chassis_classification::space_saving;
    case 16U: return chassis_classification::lunch_box;
    case 17U: return chassis_classification::main_server;
    case 18U: return chassis_classification::expansion_chassis;
    case 19U: return chassis_classification::sub_chassis;
    case 20U: return chassis_classification::bus_expansion_chassis;
    case 21U: return chassis_classification::peripheral_chassis;
    case 22U: return chassis_classification::raid_chassis;
    case 23U: return chassis_classification::rack_mount_chassis;
    case 24U: return chassis_classification::sealed_case_pc;
    case 25U: return chassis_classification::multi_system;
    case 26U: return chassis_classification::compact_pci;
    case 27U: return chassis_classification::advanced_tca;
    case 28U: return chassis_classification::blade;
    case 29U: return chassis_classification::blade_enclosure;
    case 30U: return chassis_classification::tablet;
    case 31U: return chassis_classification::convertible;
    case 32U: return chassis_classification::detachable;
    case 33U: return chassis_classification::iot_gateway;
    case 34U: return chassis_classification::embedded_pc;
    case 35U: return chassis_classification::mini_pc;
    case 36U: return chassis_classification::stick_pc;
    default: return fail(errc::malformed_data);
    }
}

/// One SMBIOS-recorded hardware UUID exactly as the table stores it.
struct uuid_octets {
    std::uint8_t value[16];
};

/// Reports whether one recorded UUID carries the SMBIOS-documented absence
/// renderings.
///
/// The specification defines every octet zero or every octet FFh as
/// recording no identifier, so callers translate both renderings into
/// not_found instead of returning a value that distinguishes nothing.
inline bool uuid_records_no_identifier(const uuid_octets& uuid) noexcept {
    bool saw_nonzero = false;
    bool saw_not_ff = false;
    for (std::size_t index = 0; index < sizeof(uuid.value); ++index) {
        if (uuid.value[index] != 0x00U) { saw_nonzero = true; }
        if (uuid.value[index] != 0xFFU) { saw_not_ff = true; }
    }
    return !saw_nonzero || !saw_not_ff;
}

/// Renders RFC 4122 fields plus the remaining octets as canonical lowercase
/// hyphenated text.
///
/// @param time_low the first four bytes assembled in presentation order.
/// @param time_mid the following two bytes in presentation order.
/// @param time_hi_and_version the next two bytes in presentation order.
/// @param remaining eight octets rendered as the last two dash-separated
/// groups of two and six bytes.
inline std::string render_canonical_uuid(std::uint32_t time_low,
                                         std::uint16_t time_mid,
                                         std::uint16_t time_hi_and_version,
                                         const std::uint8_t* remaining) {
    static constexpr char digits[] = "0123456789abcdef";
    const auto fixed = [](std::uint64_t value, std::size_t width) {
        std::string rendered(width, '0');
        for (std::size_t index = width; index-- > 0U;) {
            rendered[index] = digits[value & 0xFU];
            value >>= 4U;
        }
        return rendered;
    };
    std::string output = fixed(time_low, 8U);
    output += '-';
    output += fixed(time_mid, 4U);
    output += '-';
    output += fixed(time_hi_and_version, 4U);
    output += '-';
    output += fixed(remaining[0], 2U);
    output += fixed(remaining[1], 2U);
    output += '-';
    for (std::size_t index = 2U; index < 8U; ++index) {
        output += fixed(remaining[index], 2U);
    }
    return output;
}

/// Returns the numeric value of one lowercase or uppercase hexadecimal
/// digit, or minus one when the character cannot be part of a UUID text.
inline int hex_digit_value(char letter) noexcept {
    if (letter >= '0' && letter <= '9') { return letter - '0'; }
    if (letter >= 'a' && letter <= 'f') { return letter - 'a' + 10; }
    if (letter >= 'A' && letter <= 'F') { return letter - 'A' + 10; }
    return -1;
}

/// Validates one textual UUID rendering at the public boundary.
///
/// Accepted renderings are the thirty-six-character hyphenated form with any
/// hexadecimal letter case, which covers the kernel's product_uuid attribute
/// and stays harmless for backends that already render canonically. The
/// output re-renders the same digits in lowercase so callers compare values
/// rather than spellings. Renderings outside that shape are malformed
/// platform data, and both SMBIOS-documented absence markers report
/// not_found.
inline result<std::string> validate_uuid_text(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    static constexpr std::size_t expected_length = 36U;
    static constexpr std::size_t hyphen_positions[4] = {8U, 13U, 18U, 23U};
    if (value->size() != expected_length) {
        return fail(errc::malformed_data);
    }
    bool all_zero = true;
    bool all_ff = true;
    std::string canonical;
    canonical.reserve(expected_length);
    std::size_t position = 0U;
    while (position < expected_length) {
        bool at_hyphen = false;
        for (const std::size_t marked : hyphen_positions) {
            if (marked == position) { at_hyphen = true; break; }
        }
        if (at_hyphen) {
            if ((*value)[position] != '-') {
                return fail(errc::malformed_data);
            }
            canonical.push_back('-');
            ++position;
            continue;
        }
        const int digit = hex_digit_value((*value)[position]);
        if (digit < 0) { return fail(errc::malformed_data); }
        if (digit != 0) { all_zero = false; }
        if (digit != 0xF) { all_ff = false; }
        canonical.push_back(digit < 10 ? static_cast<char>('0' + digit)
                                       : static_cast<char>('a' + digit - 10));
        ++position;
    }
    if (all_zero || all_ff) { return fail(errc::not_found); }
    return canonical;
}

} // namespace hardware_common
} // namespace detail
} // namespace syscape

#endif
