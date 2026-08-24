#ifndef SYSCAPE_DETAIL_HARDWARE_WINDOWS_HPP
#define SYSCAPE_DETAIL_HARDWARE_WINDOWS_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <windows.h>

#include <syscape/detail/hardware/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_backend {

/// Documented provider identifier of the raw-SMBIOS firmware-table provider,
/// rendered from the four characters 'R', 'S', 'M', 'B' in their documented
/// little-endian order.
constexpr ::DWORD smbios_provider = 0x52534D42UL;

/// Internal mirror of the documented RAW_SMBIOSData layout returned by
/// GetSystemFirmwareTable('RSMB', ...) for the raw SMBIOS provider.
///
/// The structure is defined locally so that no SDK-version-dependent
/// declaration is required, and every multi-byte field is read through
/// memcpy because its alignment inside the returned buffer is not guaranteed
/// by the interface documentation.
struct raw_smbios_header {
    /// Whether the data was obtained through the SMBIOS 2.0 calling method.
    std::uint8_t used_20_calling_method = 0U;
    /// Major version of the SMBIOS specification the table follows.
    std::uint8_t major_version = 0U;
    /// Minor version of the SMBIOS specification the table follows.
    std::uint8_t minor_version = 0U;
    /// Firmware-table revision reported by the interface.
    std::uint8_t dmi_revision = 0U;
    /// Size in bytes of the SMBIOS structure table that follows.
    std::uint32_t length = 0U;
};

/// Number of bytes the raw-SMBIOS header occupies ahead of the table.
constexpr std::size_t raw_smbios_header_size = 8U;

/// Upper bound on accepted table sizes. Real SMBIOS tables are far smaller;
/// the cap keeps a misbehaving interface from producing unbounded buffers.
constexpr std::size_t maximum_table_size = 1024U * 1024U;

/// Documented structure-type rendering that ends an SMBIOS table.
constexpr std::uint8_t end_of_table_type = 127U;

inline std::error_code last_error() noexcept {
    return std::error_code(static_cast<int>(::GetLastError()),
                           std::system_category());
}

/// View over one parsed SMBIOS structure inside a table buffer.
struct structure_view {
    /// Start of the formatted area, header included.
    const std::uint8_t* formatted = nullptr;
    /// Total size in bytes of the formatted area.
    std::uint32_t formatted_size = 0U;
    /// First byte of the string area, which is either a string body or the
    /// terminating null of an empty string area.
    const std::uint8_t* strings = nullptr;
    /// Number of bytes between the first string byte and the double-null
    /// terminator, separators included.
    std::uint32_t strings_size = 0U;
};

/// One indexed string copied out of a structure's string area.
///
/// An absent recording covers both the documented index zero, which names no
/// string, and a present-but-empty string body, because presenting emptiness
/// would present nothing as data.
struct extracted_string {
    bool present = false;
    std::string value;
};

/// Copies the one-based indexed string of one structure.
///
/// String indices are valid by construction inside a conforming table, so an
/// index beyond the recorded string count contradicts the specification's
/// own indexing and fails as malformed platform data instead of being
/// silently ignored.
inline result<extracted_string> extract_string(
    const structure_view& structure, std::uint8_t index) {
    extracted_string outcome;
    if (index == 0U) { return outcome; }
    std::size_t start = 0U;
    for (std::uint8_t seen = 1U;; ++seen) {
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

/// Stores one extracted string into an optional fact slot.
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

/// The plain facts this module derives from one raw SMBIOS table.
///
/// The first record of each relevant structure type wins because later
/// duplicates describe add-on components rather than the primary system,
/// board, enclosure, or firmware.
struct identity_facts {
    bool has_firmware_vendor = false;
    std::string firmware_vendor_value;
    bool has_firmware_version = false;
    std::string firmware_version_value;
    bool has_firmware_release_date = false;
    std::string firmware_release_date_value;

    bool has_system_manufacturer = false;
    std::string system_manufacturer_value;
    bool has_system_product_name = false;
    std::string system_product_name_value;
    bool has_system_product_version = false;
    std::string system_product_version_value;
    /// Whether the system record carries the sixteen-byte UUID field.
    bool has_uuid = false;
    hardware_common::uuid_octets uuid;

    bool has_board_manufacturer = false;
    std::string board_manufacturer_value;
    bool has_board_product_name = false;
    std::string board_product_name_value;
    bool has_board_version = false;
    std::string board_version_value;

    /// Whether the enclosure record carries its classification byte.
    bool has_chassis_type = false;
    std::uint8_t chassis_type_value = 0U;
};

/// Walks one raw SMBIOS structure table and reduces it to plain facts.
///
/// The walk validates every record boundary against the caller-provided
/// size, so truncated or unterminated tables fail as malformed platform data
/// instead of reading out of bounds.
inline result<identity_facts> parse_smbios_table(const std::uint8_t* data,
                                                 std::size_t size) {
    identity_facts facts;
    std::size_t cursor = 0U;
    bool saw_firmware = false;
    bool saw_system = false;
    bool saw_board = false;
    bool saw_chassis = false;
    while (cursor < size) {
        if (size - cursor < 4U) { return fail(errc::malformed_data); }
        const std::uint8_t type = data[cursor];
        if (type == end_of_table_type) { break; }
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

        if (type == 0U && !saw_firmware) {
            saw_firmware = true;
            // The BIOS Information record requires at least 18 formatted
            // bytes so that the vendor, version, and release-date indices at
            // offsets 04h, 05h, and 08h exist.
            if (formatted_size < 0x12U) {
                return fail(errc::malformed_data);
            }
            const result<void> stored_vendor = store_string(
                view, view.formatted[4], facts.has_firmware_vendor,
                facts.firmware_vendor_value);
            if (!stored_vendor) { return fail(stored_vendor.error()); }
            const result<void> stored_version = store_string(
                view, view.formatted[5], facts.has_firmware_version,
                facts.firmware_version_value);
            if (!stored_version) { return fail(stored_version.error()); }
            const result<void> stored_date = store_string(
                view, view.formatted[8], facts.has_firmware_release_date,
                facts.firmware_release_date_value);
            if (!stored_date) { return fail(stored_date.error()); }
        } else if (type == 1U && !saw_system) {
            saw_system = true;
            // System Information records of every specification revision
            // carry at least eight formatted bytes, which cover the
            // manufacturer, product-name, and version indices. The
            // sixteen-byte UUID field exists only when the record extends to
            // offset 17h inclusive.
            if (formatted_size < 0x08U) {
                return fail(errc::malformed_data);
            }
            const result<void> stored_manufacturer = store_string(
                view, view.formatted[4], facts.has_system_manufacturer,
                facts.system_manufacturer_value);
            if (!stored_manufacturer) {
                return fail(stored_manufacturer.error());
            }
            const result<void> stored_product = store_string(
                view, view.formatted[5], facts.has_system_product_name,
                facts.system_product_name_value);
            if (!stored_product) { return fail(stored_product.error()); }
            const result<void> stored_version = store_string(
                view, view.formatted[6], facts.has_system_product_version,
                facts.system_product_version_value);
            if (!stored_version) { return fail(stored_version.error()); }
            if (formatted_size >= 0x18U) {
                std::memcpy(facts.uuid.value, view.formatted + 0x08U,
                            sizeof(facts.uuid.value));
                facts.has_uuid = true;
            }
        } else if (type == 2U && !saw_board) {
            saw_board = true;
            // Base Board records carry at least eight formatted bytes, which
            // cover the manufacturer, product-name, and version indices.
            if (formatted_size < 0x08U) {
                return fail(errc::malformed_data);
            }
            const result<void> stored_manufacturer = store_string(
                view, view.formatted[4], facts.has_board_manufacturer,
                facts.board_manufacturer_value);
            if (!stored_manufacturer) {
                return fail(stored_manufacturer.error());
            }
            const result<void> stored_product = store_string(
                view, view.formatted[5], facts.has_board_product_name,
                facts.board_product_name_value);
            if (!stored_product) { return fail(stored_product.error()); }
            const result<void> stored_version = store_string(
                view, view.formatted[6], facts.has_board_version,
                facts.board_version_value);
            if (!stored_version) { return fail(stored_version.error()); }
        } else if (type == 3U && !saw_chassis) {
            saw_chassis = true;
            // System Enclosure records carry at least nine formatted bytes,
            // which cover the manufacturer index and the classification byte
            // whose lower seven bits render the documented chassis type.
            if (formatted_size < 0x09U) {
                return fail(errc::malformed_data);
            }
            facts.chassis_type_value =
                static_cast<std::uint8_t>(view.formatted[5] & 0x7FU);
            facts.has_chassis_type = true;
        }

        cursor = strings_offset + view.strings_size + 2U;
    }
    return facts;
}

/// Reads one raw SMBIOS table through the documented two-stage sizing call.
inline result<std::vector<std::uint8_t>> fetch_table() {
    const ::UINT required =
        ::GetSystemFirmwareTable(smbios_provider, 0U, nullptr, 0U);
    if (required == 0U) { return fail(last_error()); }
    if (static_cast<std::size_t>(required) > maximum_table_size) {
        return fail(errc::value_too_large);
    }
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(required));
    const ::UINT written = ::GetSystemFirmwareTable(
        smbios_provider, 0U, buffer.data(), required);
    if (written == 0U) { return fail(last_error()); }
    if (static_cast<std::size_t>(written) > buffer.size()) {
        return fail(errc::malformed_data);
    }
    buffer.resize(written);
    if (buffer.size() <= raw_smbios_header_size) {
        return fail(errc::malformed_data);
    }
    return buffer;
}

/// Fetches one table snapshot and reduces it to plain facts.
inline result<identity_facts> collect_identity_facts() {
    const result<std::vector<std::uint8_t>> table = fetch_table();
    if (!table) { return fail(table.error()); }
    raw_smbios_header header;
    header.used_20_calling_method = (*table)[0];
    header.major_version = (*table)[1];
    header.minor_version = (*table)[2];
    header.dmi_revision = (*table)[3];
    std::memcpy(&header.length, table->data() + 4U, sizeof(header.length));
    if (header.length >
        static_cast<std::uint32_t>(table->size() - raw_smbios_header_size)) {
        return fail(errc::malformed_data);
    }
    return parse_smbios_table(
        table->data() + raw_smbios_header_size, header.length);
}

/// Reduces one optional text fact into a query answer.
inline result<std::string> interpret_text(bool present,
                                          const std::string& value) {
    if (!present) { return fail(errc::not_found); }
    return value;
}

inline result<hardware_common::chassis_classification> interpret_chassis(
    const identity_facts& facts) {
    if (!facts.has_chassis_type) { return fail(errc::not_found); }
    return hardware_common::classify_chassis(facts.chassis_type_value);
}

/// Renders the recorded UUID as canonical lowercase text.
///
/// Since SMBIOS 2.6 the first three RFC 4122 fields are transmitted
/// little-endian, so they are reassembled here before rendering; the
/// remaining octets keep their recorded order. Both absence renderings the
/// specification defines report not_found rather than a value that
/// distinguishes nothing.
inline result<std::string> interpret_uuid(const identity_facts& facts) {
    if (!facts.has_uuid) { return fail(errc::not_found); }
    if (hardware_common::uuid_records_no_identifier(facts.uuid)) {
        return fail(errc::not_found);
    }
    const std::uint8_t* bytes = facts.uuid.value;
    const std::uint32_t time_low =
        static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[3]) << 24U);
    const std::uint16_t time_mid = static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(bytes[4]) |
        (static_cast<std::uint32_t>(bytes[5]) << 8U));
    const std::uint16_t time_hi_and_version = static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(bytes[6]) |
        (static_cast<std::uint32_t>(bytes[7]) << 8U));
    return hardware_common::render_canonical_uuid(
        time_low, time_mid, time_hi_and_version, bytes + 8U);
}

inline result<std::string> system_manufacturer() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_system_manufacturer,
                          facts->system_manufacturer_value);
}

inline result<std::string> system_product_name() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_system_product_name,
                          facts->system_product_name_value);
}

inline result<std::string> system_product_version() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_system_product_version,
                          facts->system_product_version_value);
}

inline result<std::string> motherboard_manufacturer() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_board_manufacturer,
                          facts->board_manufacturer_value);
}

inline result<std::string> motherboard_product_name() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_board_product_name,
                          facts->board_product_name_value);
}

inline result<std::string> motherboard_version() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_board_version,
                          facts->board_version_value);
}

inline result<std::string> firmware_vendor() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_firmware_vendor,
                          facts->firmware_vendor_value);
}

inline result<std::string> firmware_version() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_firmware_version,
                          facts->firmware_version_value);
}

inline result<std::string> firmware_release_date() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_text(facts->has_firmware_release_date,
                          facts->firmware_release_date_value);
}

inline result<hardware_common::chassis_classification> chassis_form_factor() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_chassis(*facts);
}

inline result<std::string> hardware_uuid() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_uuid(*facts);
}

} // namespace hardware_backend
} // namespace detail
} // namespace syscape

#endif
