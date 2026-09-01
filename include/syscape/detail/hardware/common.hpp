#ifndef SYSCAPE_DETAIL_HARDWARE_COMMON_HPP
#define SYSCAPE_DETAIL_HARDWARE_COMMON_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_common {

using chassis_classification = ::syscape::hardware::form_factor;
using pci_class_type = ::syscape::hardware::pci_class;
using memory_form_factor_type = ::syscape::hardware::memory_form_factor;
using memory_tech_type = ::syscape::hardware::memory_type;

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

/// Maps one PCI base class code onto the shared pci_class vocabulary.
inline pci_class_type classify_pci_class(std::uint8_t class_code) noexcept {
    switch (class_code) {
    case 0x00U: return pci_class_type::unclassified;
    case 0x01U: return pci_class_type::mass_storage;
    case 0x02U: return pci_class_type::network_controller;
    case 0x03U: return pci_class_type::display_controller;
    case 0x04U: return pci_class_type::multimedia_controller;
    case 0x05U: return pci_class_type::memory_controller;
    case 0x06U: return pci_class_type::bridge;
    case 0x07U: return pci_class_type::communication_controller;
    case 0x08U: return pci_class_type::generic_system_peripheral;
    case 0x09U: return pci_class_type::input_device_controller;
    case 0x0AU: return pci_class_type::docking_station;
    case 0x0BU: return pci_class_type::processor;
    case 0x0CU: return pci_class_type::serial_bus_controller;
    case 0x0DU: return pci_class_type::wireless_controller;
    case 0x0EU: return pci_class_type::intelligent_controller;
    case 0x0FU: return pci_class_type::satellite_communication;
    case 0x10U: return pci_class_type::encryption_controller;
    case 0x11U: return pci_class_type::signal_processing_controller;
    case 0x12U: return pci_class_type::processing_accelerator;
    case 0x13U: return pci_class_type::non_essential_instrumentation;
    default: return pci_class_type::unknown;
    }
}

/// Maps one SMBIOS Type 17 Form Factor byte onto the memory_form_factor vocabulary.
inline memory_form_factor_type classify_memory_form_factor(
    std::uint8_t recorded) noexcept {
    switch (recorded) {
    case 0x01U: return memory_form_factor_type::other;
    case 0x02U: return memory_form_factor_type::unknown;
    case 0x03U: return memory_form_factor_type::simm;
    case 0x04U: return memory_form_factor_type::sip;
    case 0x05U: return memory_form_factor_type::chip;
    case 0x06U: return memory_form_factor_type::dip;
    case 0x07U: return memory_form_factor_type::zip;
    case 0x08U: return memory_form_factor_type::soj;
    case 0x09U: return memory_form_factor_type::proprietary;
    case 0x0AU: return memory_form_factor_type::dimm;
    case 0x0BU: return memory_form_factor_type::tsop;
    case 0x0CU: return memory_form_factor_type::row_of_chips;
    case 0x0DU: return memory_form_factor_type::rimm;
    case 0x0EU: return memory_form_factor_type::sodimm;
    case 0x0FU: return memory_form_factor_type::srimm;
    case 0x10U: return memory_form_factor_type::fb_dimm;
    case 0x11U: return memory_form_factor_type::die;
    case 0x12U: return memory_form_factor_type::camm;
    default: return memory_form_factor_type::unknown;
    }
}

/// Maps one SMBIOS Type 17 Memory Type byte onto the memory_type vocabulary.
inline memory_tech_type classify_memory_type(
    std::uint8_t recorded) noexcept {
    switch (recorded) {
    case 0x01U: return memory_tech_type::other;
    case 0x02U: return memory_tech_type::unknown;
    case 0x03U: return memory_tech_type::dram;
    case 0x04U: return memory_tech_type::edram;
    case 0x05U: return memory_tech_type::vram;
    case 0x06U: return memory_tech_type::sram;
    case 0x07U: return memory_tech_type::ram;
    case 0x08U: return memory_tech_type::rom;
    case 0x09U: return memory_tech_type::flash;
    case 0x0AU: return memory_tech_type::eeprom;
    case 0x0BU: return memory_tech_type::feprom;
    case 0x0CU: return memory_tech_type::eprom;
    case 0x0DU: return memory_tech_type::cdram;
    case 0x0EU: return memory_tech_type::three_d_ram;
    case 0x0FU: return memory_tech_type::sdram;
    case 0x10U: return memory_tech_type::sgram;
    case 0x11U: return memory_tech_type::rdram;
    case 0x12U: return memory_tech_type::ddr;
    case 0x13U: return memory_tech_type::ddr2;
    case 0x14U: return memory_tech_type::ddr2_fb_dimm;
    case 0x18U: return memory_tech_type::ddr3;
    case 0x19U: return memory_tech_type::fbd2;
    case 0x1AU: return memory_tech_type::ddr4;
    case 0x1BU: return memory_tech_type::lpddr;
    case 0x1CU: return memory_tech_type::lpddr2;
    case 0x1DU: return memory_tech_type::lpddr3;
    case 0x1EU: return memory_tech_type::lpddr4;
    case 0x1FU: return memory_tech_type::logical_non_volatile;
    case 0x20U: return memory_tech_type::hbm;
    case 0x21U: return memory_tech_type::hbm2;
    case 0x22U: return memory_tech_type::ddr5;
    case 0x23U: return memory_tech_type::lpddr5;
    case 0x24U: return memory_tech_type::hbm3;
    default: return memory_tech_type::unknown;
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

/// View over one parsed SMBIOS structure inside a table buffer.
struct structure_view {
    /// Start of the formatted area, header included.
    const std::uint8_t* formatted = nullptr;
    /// Total size in bytes of the formatted area.
    std::uint32_t formatted_size = 0U;
    /// First byte of the string area.
    const std::uint8_t* strings = nullptr;
    /// Number of bytes between the first string byte and the double-null terminator.
    std::uint32_t strings_size = 0U;
};

/// One indexed string copied out of a structure's string area.
struct extracted_string {
    bool present = false;
    std::string value;
};

/// Copies the one-based indexed string of one structure.
inline result<extracted_string> extract_string(
    const structure_view& structure, std::uint8_t index) {
    extracted_string outcome;
    if (index == 0U) { return outcome; }
    std::size_t start = 0U;
    for (std::uint8_t seen = 1U;; ++seen) {
        if (start >= structure.strings_size) {
            return fail(errc::malformed_data);
        }
        std::size_t end = start;
        while (end < structure.strings_size &&
               structure.strings[end] != 0U) {
            ++end;
        }
        if (seen == index) {
            if (end > start) {
                outcome.present = true;
                outcome.value.assign(
                    reinterpret_cast<const char*>(structure.strings + start),
                    end - start);
            }
            return outcome;
        }
        if (end >= structure.strings_size) {
            return fail(errc::malformed_data);
        }
        start = end + 1U;
    }
}

/// Stores one extracted string into destination if present.
inline result<void> store_string(const structure_view& structure,
                                 std::uint8_t index, bool& present,
                                 std::string& destination) {
    const result<extracted_string> extracted =
        extract_string(structure, index);
    if (!extracted) { return fail(extracted.error()); }
    present = extracted->present;
    if (extracted->present) {
        destination = std::move(extracted->value);
    } else {
        destination.clear();
    }
    return {};
}

/// Ordering comparison for deterministic PCI device sorting.
inline bool compare_pci_devices(const ::syscape::hardware::pci_device& a,
                                const ::syscape::hardware::pci_device& b) noexcept {
    if (a.domain != b.domain) { return a.domain < b.domain; }
    if (a.bus != b.bus) { return a.bus < b.bus; }
    if (a.device != b.device) { return a.device < b.device; }
    if (a.function != b.function) { return a.function < b.function; }
    if (a.vendor_id != b.vendor_id) { return a.vendor_id < b.vendor_id; }
    if (a.device_id != b.device_id) { return a.device_id < b.device_id; }
    if (a.subsystem_vendor_id != b.subsystem_vendor_id) {
        return a.subsystem_vendor_id < b.subsystem_vendor_id;
    }
    if (a.subsystem_device_id != b.subsystem_device_id) {
        return a.subsystem_device_id < b.subsystem_device_id;
    }
    if (a.class_code != b.class_code) { return a.class_code < b.class_code; }
    if (a.subclass_code != b.subclass_code) {
        return a.subclass_code < b.subclass_code;
    }
    if (a.programming_interface != b.programming_interface) {
        return a.programming_interface < b.programming_interface;
    }
    if (a.driver != b.driver) { return a.driver < b.driver; }
    return a.slot_name < b.slot_name;
}

/// Ordering comparison for deterministic USB device sorting.
inline bool compare_usb_devices(const ::syscape::hardware::usb_device& a,
                                const ::syscape::hardware::usb_device& b) noexcept {
    if (a.bus_number != b.bus_number) { return a.bus_number < b.bus_number; }
    if (a.device_address != b.device_address) {
        return a.device_address < b.device_address;
    }
    if (a.port_number != b.port_number) {
        return a.port_number < b.port_number;
    }
    if (a.vendor_id != b.vendor_id) { return a.vendor_id < b.vendor_id; }
    if (a.product_id != b.product_id) { return a.product_id < b.product_id; }
    if (a.bcd_device != b.bcd_device) { return a.bcd_device < b.bcd_device; }
    if (a.device_class != b.device_class) {
        return a.device_class < b.device_class;
    }
    if (a.device_subclass != b.device_subclass) {
        return a.device_subclass < b.device_subclass;
    }
    if (a.device_protocol != b.device_protocol) {
        return a.device_protocol < b.device_protocol;
    }
    if (a.manufacturer != b.manufacturer) {
        return a.manufacturer < b.manufacturer;
    }
    if (a.product != b.product) { return a.product < b.product; }
    return a.serial_number < b.serial_number;
}

/// Ordering comparison for deterministic memory slot/module sorting.
inline bool compare_memory_devices(const ::syscape::hardware::memory_device& a,
                                   const ::syscape::hardware::memory_device& b) noexcept {
    if (a.locator != b.locator) { return a.locator < b.locator; }
    if (a.bank_locator != b.bank_locator) {
        return a.bank_locator < b.bank_locator;
    }
    if (a.state != b.state) {
        return static_cast<std::uint8_t>(a.state) <
            static_cast<std::uint8_t>(b.state);
    }
    if (a.size_bytes != b.size_bytes) { return a.size_bytes < b.size_bytes; }
    if (a.manufacturer != b.manufacturer) {
        return a.manufacturer < b.manufacturer;
    }
    if (a.serial_number != b.serial_number) {
        return a.serial_number < b.serial_number;
    }
    return a.part_number < b.part_number;
}

/// Reads one little-endian 16-bit SMBIOS field without host-endian assumptions.
inline std::uint16_t read_le_u16(const std::uint8_t* value) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(value[0]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(value[1]) << 8U));
}

/// Reads one little-endian 32-bit SMBIOS field without host-endian assumptions.
inline std::uint32_t read_le_u32(const std::uint8_t* value) noexcept {
    return static_cast<std::uint32_t>(value[0]) |
        (static_cast<std::uint32_t>(value[1]) << 8U) |
        (static_cast<std::uint32_t>(value[2]) << 16U) |
        (static_cast<std::uint32_t>(value[3]) << 24U);
}

/// Parses SMBIOS Type 17 memory device records from a raw SMBIOS structure table.
inline result<std::vector<::syscape::hardware::memory_device>>
parse_smbios_memory_devices(
    const std::uint8_t* data, std::size_t size,
    ::syscape::hardware::memory_speed_unit speed_unit =
        ::syscape::hardware::memory_speed_unit::unknown) {
    if (data == nullptr || size == 0U) { return fail(errc::malformed_data); }
    std::vector<::syscape::hardware::memory_device> devices;
    std::size_t cursor = 0U;
    bool saw_end = false;
    while (cursor < size) {
        if (size - cursor < 4U) { return fail(errc::malformed_data); }
        const std::uint8_t type = data[cursor];
        const std::size_t formatted_size = data[cursor + 1U];
        if (formatted_size < 4U || formatted_size > size - cursor) {
            return fail(errc::malformed_data);
        }

        structure_view view;
        view.formatted = data + cursor;
        view.formatted_size = static_cast<std::uint32_t>(formatted_size);

        const std::size_t strings_offset = cursor + formatted_size;
        std::size_t scan = strings_offset;
        while (scan + 1U < size &&
               !(data[scan] == 0U && data[scan + 1U] == 0U)) {
            ++scan;
        }
        if (scan + 1U >= size) { return fail(errc::malformed_data); }
        view.strings = data + strings_offset;
        view.strings_size =
            static_cast<std::uint32_t>(scan - strings_offset);

        if (type == 127U) { // end of table marker
            if (formatted_size != 4U) { return fail(errc::malformed_data); }
            const std::size_t trailing = scan + 2U;
            for (std::size_t index = trailing; index < size; ++index) {
                if (data[index] != 0U) { return fail(errc::malformed_data); }
            }
            saw_end = true;
            break;
        }

        if (type == 17U) { // Type 17: Memory Device
            if (formatted_size < 0x15U) {
                return fail(errc::malformed_data);
            }
            ::syscape::hardware::memory_device dev;

            // Offset 10h: Device Locator string index
            bool has_locator = false;
            const result<void> stored_loc = store_string(
                view, view.formatted[0x10U], has_locator, dev.locator);
            if (!stored_loc) { return fail(stored_loc.error()); }

            // Offset 11h: Bank Locator string index
            bool has_bank = false;
            std::string bank_str;
            const result<void> stored_bank = store_string(
                view, view.formatted[0x11U], has_bank, bank_str);
            if (!stored_bank) { return fail(stored_bank.error()); }
            if (has_bank && !bank_str.empty()) {
                dev.bank_locator = std::move(bank_str);
            }

            // Offset 0Ch: Size (2 bytes)
            const std::uint16_t raw_size = read_le_u16(view.formatted + 0x0CU);

            if (raw_size == 0x0000U) {
                dev.state = ::syscape::hardware::memory_device_state::not_installed;
                dev.size_bytes = std::nullopt;
            } else if (raw_size == 0xFFFFU) {
                dev.state = ::syscape::hardware::memory_device_state::installed;
                dev.size_bytes = std::nullopt;
            } else if (raw_size == 0x7FFFU) {
                if (formatted_size < 0x20U) { return fail(errc::malformed_data); }
                const std::uint32_t ext_size = read_le_u32(view.formatted + 0x1CU);
                if ((ext_size & 0x80000000U) != 0U || ext_size == 0U) {
                    return fail(errc::malformed_data);
                }
                dev.state = ::syscape::hardware::memory_device_state::installed;
                const std::uint64_t size_mb = ext_size;
                dev.size_bytes = size_mb * 1024ULL * 1024ULL;
            } else if ((raw_size & 0x8000U) != 0U) {
                const std::uint64_t size_kb = raw_size & 0x7FFFU;
                if (size_kb == 0U) { return fail(errc::malformed_data); }
                dev.state = ::syscape::hardware::memory_device_state::installed;
                dev.size_bytes = size_kb * 1024ULL;
            } else {
                const std::uint64_t size_mb = raw_size;
                dev.state = ::syscape::hardware::memory_device_state::installed;
                dev.size_bytes = size_mb * 1024ULL * 1024ULL;
            }

            // Offset 0Eh: Form Factor
            dev.form_factor = classify_memory_form_factor(view.formatted[0x0EU]);

            // Offset 12h: Memory Type
            dev.type = classify_memory_type(view.formatted[0x12U]);

            // Offset 15h: Speed (SMBIOS 2.3+)
            if (formatted_size >= 0x17U) {
                const std::uint16_t speed = read_le_u16(view.formatted + 0x15U);
                if (speed != 0U && speed != 0xFFFFU) {
                    dev.speed = ::syscape::hardware::memory_speed{
                        static_cast<std::uint32_t>(speed), speed_unit};
                } else if (speed == 0xFFFFU && formatted_size >= 0x58U) {
                    const std::uint32_t ext_speed = read_le_u32(view.formatted + 0x54U);
                    if ((ext_speed & 0x80000000U) != 0U) {
                        return fail(errc::malformed_data);
                    }
                    if (ext_speed != 0U) {
                        dev.speed = ::syscape::hardware::memory_speed{
                            ext_speed, speed_unit};
                    }
                }
            }

            // Offset 17h: Manufacturer (SMBIOS 2.3+)
            if (formatted_size >= 0x18U) {
                bool has_mfg = false;
                std::string mfg_str;
                const result<void> stored_mfg = store_string(
                    view, view.formatted[0x17U], has_mfg, mfg_str);
                if (!stored_mfg) { return fail(stored_mfg.error()); }
                if (has_mfg && !mfg_str.empty()) {
                    dev.manufacturer = std::move(mfg_str);
                }
            }

            // Offset 18h: Serial Number (SMBIOS 2.3+)
            if (formatted_size >= 0x19U) {
                bool has_sn = false;
                std::string sn_str;
                const result<void> stored_sn = store_string(
                    view, view.formatted[0x18U], has_sn, sn_str);
                if (!stored_sn) { return fail(stored_sn.error()); }
                if (has_sn && !sn_str.empty()) {
                    dev.serial_number = std::move(sn_str);
                }
            }

            // Offset 1Ah: Part Number (SMBIOS 2.3+)
            if (formatted_size >= 0x1BU) {
                bool has_part = false;
                std::string part_str;
                const result<void> stored_part = store_string(
                    view, view.formatted[0x1AU], has_part, part_str);
                if (!stored_part) { return fail(stored_part.error()); }
                if (has_part && !part_str.empty()) {
                    dev.part_number = std::move(part_str);
                }
            }

            // Offset 20h: Configured Memory Speed (SMBIOS 2.7+)
            if (formatted_size >= 0x22U) {
                const std::uint16_t conf_speed = read_le_u16(view.formatted + 0x20U);
                if (conf_speed != 0U && conf_speed != 0xFFFFU) {
                    dev.configured_speed = ::syscape::hardware::memory_speed{
                        static_cast<std::uint32_t>(conf_speed), speed_unit};
                } else if (conf_speed == 0xFFFFU && formatted_size >= 0x5CU) {
                    const std::uint32_t ext_conf = read_le_u32(view.formatted + 0x58U);
                    if ((ext_conf & 0x80000000U) != 0U) {
                        return fail(errc::malformed_data);
                    }
                    if (ext_conf != 0U) {
                        dev.configured_speed = ::syscape::hardware::memory_speed{
                            ext_conf, speed_unit};
                    }
                }
            }

            devices.push_back(std::move(dev));
        }

        cursor = strings_offset + view.strings_size + 2U;
    }
    if (!saw_end) { return fail(errc::malformed_data); }
    std::sort(devices.begin(), devices.end(), compare_memory_devices);
    return devices;
}

/// Validates UTF-8 text in PCI device records at the public boundary.
inline result<std::vector<::syscape::hardware::pci_device>> validate_pci_devices(
    result<std::vector<::syscape::hardware::pci_device>> devices) {
    if (!devices) { return fail(devices.error()); }
    for (const auto& dev : *devices) {
        if (dev.driver && !is_valid_utf8(*dev.driver)) {
            return fail(errc::invalid_encoding);
        }
        if (dev.slot_name && !is_valid_utf8(*dev.slot_name)) {
            return fail(errc::invalid_encoding);
        }
    }
    return devices;
}

/// Validates UTF-8 text in USB device records at the public boundary.
inline result<std::vector<::syscape::hardware::usb_device>> validate_usb_devices(
    result<std::vector<::syscape::hardware::usb_device>> devices) {
    if (!devices) { return fail(devices.error()); }
    for (const auto& dev : *devices) {
        if (dev.manufacturer && !is_valid_utf8(*dev.manufacturer)) {
            return fail(errc::invalid_encoding);
        }
        if (dev.product && !is_valid_utf8(*dev.product)) {
            return fail(errc::invalid_encoding);
        }
        if (dev.serial_number && !is_valid_utf8(*dev.serial_number)) {
            return fail(errc::invalid_encoding);
        }
    }
    return devices;
}

/// Validates UTF-8 text in memory device records at the public boundary.
inline result<std::vector<::syscape::hardware::memory_device>> validate_memory_devices(
    result<std::vector<::syscape::hardware::memory_device>> devices) {
    if (!devices) { return fail(devices.error()); }
    for (const auto& dev : *devices) {
        if (!is_valid_utf8(dev.locator)) {
            return fail(errc::invalid_encoding);
        }
        if (dev.bank_locator && !is_valid_utf8(*dev.bank_locator)) {
            return fail(errc::invalid_encoding);
        }
        if (dev.manufacturer && !is_valid_utf8(*dev.manufacturer)) {
            return fail(errc::invalid_encoding);
        }
        if (dev.serial_number && !is_valid_utf8(*dev.serial_number)) {
            return fail(errc::invalid_encoding);
        }
        if (dev.part_number && !is_valid_utf8(*dev.part_number)) {
            return fail(errc::invalid_encoding);
        }
    }
    return devices;
}

} // namespace hardware_common
} // namespace detail
} // namespace syscape

#endif
