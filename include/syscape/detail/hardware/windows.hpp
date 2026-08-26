#ifndef SYSCAPE_DETAIL_HARDWARE_WINDOWS_HPP
#define SYSCAPE_DETAIL_HARDWARE_WINDOWS_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <windows.h>
#include <setupapi.h>

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
struct identity_facts {
    /// SMBIOS specification version governing version-dependent fields.
    std::uint8_t major_version = 0U;
    std::uint8_t minor_version = 0U;

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
    /// Multiple board records existed but did not identify one motherboard.
    bool board_selection_ambiguous = false;

    /// Whether the enclosure record carries its classification byte.
    bool has_chassis_type = false;
    std::uint8_t chassis_type_value = 0U;
    /// Multiple enclosure records existed without one motherboard
    /// association.
    bool chassis_selection_ambiguous = false;
};

/// One Baseboard Information candidate retained until all records are known.
struct board_candidate {
    bool has_manufacturer = false;
    std::string manufacturer;
    bool has_product_name = false;
    std::string product_name;
    bool has_version = false;
    std::string version;
    std::uint16_t handle = 0U;
    bool identifies_motherboard = false;
    bool has_chassis_handle = false;
    std::uint16_t chassis_handle = 0U;
    bool has_containment_fields = false;
};

/// One System Enclosure candidate retained for motherboard association.
struct chassis_candidate {
    std::uint16_t handle = 0U;
    std::uint8_t type = 0U;
};

/// Walks one raw SMBIOS structure table and reduces it to plain facts.
///
/// The walk validates every record boundary against the caller-provided
/// size, so truncated or unterminated tables fail as malformed platform data
/// instead of reading out of bounds.
inline result<identity_facts> parse_smbios_table(
    const std::uint8_t* data, std::size_t size,
    std::uint8_t major_version = 0U,
    std::uint8_t minor_version = 0U) {
    identity_facts facts;
    facts.major_version = major_version;
    facts.minor_version = minor_version;
    std::size_t cursor = 0U;
    bool saw_firmware = false;
    bool saw_system = false;
    bool saw_end = false;
    std::vector<board_candidate> boards;
    std::vector<chassis_candidate> chassis_records;
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

        if (type == end_of_table_type) {
            saw_end = true;
            break;
        }

        if (type == 0U) {
            if (saw_firmware) { return fail(errc::malformed_data); }
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
        } else if (type == 1U) {
            if (saw_system) { return fail(errc::malformed_data); }
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
        } else if (type == 2U) {
            // Base Board records carry at least eight formatted bytes, which
            // cover the manufacturer, product-name, and version indices.
            if (formatted_size < 0x08U) {
                return fail(errc::malformed_data);
            }
            board_candidate candidate;
            std::memcpy(&candidate.handle, view.formatted + 2U,
                        sizeof(candidate.handle));
            const result<void> stored_manufacturer = store_string(
                view, view.formatted[4], candidate.has_manufacturer,
                candidate.manufacturer);
            if (!stored_manufacturer) {
                return fail(stored_manufacturer.error());
            }
            const result<void> stored_product = store_string(
                view, view.formatted[5], candidate.has_product_name,
                candidate.product_name);
            if (!stored_product) { return fail(stored_product.error()); }
            const result<void> stored_version = store_string(
                view, view.formatted[6], candidate.has_version,
                candidate.version);
            if (!stored_version) { return fail(stored_version.error()); }
            if (formatted_size >= 0x0AU &&
                (view.formatted[9] & 0x01U) != 0U) {
                candidate.identifies_motherboard = true;
            }
            if (formatted_size >= 0x0DU) {
                candidate.has_chassis_handle = true;
                std::memcpy(&candidate.chassis_handle,
                            view.formatted + 0x0BU,
                            sizeof(candidate.chassis_handle));
            }
            if (formatted_size >= 0x0EU && view.formatted[0x0DU] == 0x0AU) {
                candidate.identifies_motherboard = true;
            }
            if (formatted_size >= 0x0FU) {
                const std::size_t contained = view.formatted[0x0EU];
                const std::size_t required = 0x0FU + contained * 2U;
                if (required > formatted_size) {
                    return fail(errc::malformed_data);
                }
                candidate.has_containment_fields = true;
            }
            boards.push_back(std::move(candidate));
        } else if (type == 3U) {
            // System Enclosure records carry at least nine formatted bytes,
            // which cover the manufacturer index and the classification byte
            // whose lower seven bits render the documented chassis type.
            if (formatted_size < 0x09U) {
                return fail(errc::malformed_data);
            }
            chassis_candidate candidate;
            std::memcpy(&candidate.handle, view.formatted + 2U,
                        sizeof(candidate.handle));
            candidate.type =
                static_cast<std::uint8_t>(view.formatted[5] & 0x7FU);
            chassis_records.push_back(candidate);
        }

        cursor = strings_offset + view.strings_size + 2U;
    }

    const bool requires_end = major_version > 2U ||
        (major_version == 2U && minor_version >= 2U);
    if (requires_end && !saw_end) { return fail(errc::malformed_data); }

    const board_candidate* selected_board = nullptr;
    if (boards.size() == 1U) {
        selected_board = &boards.front();
    } else if (!boards.empty()) {
        std::size_t identified = 0U;
        for (const board_candidate& board : boards) {
            if (!board.has_containment_fields) {
                return fail(errc::malformed_data);
            }
            if (board.identifies_motherboard) {
                selected_board = &board;
                ++identified;
            }
        }
        if (identified != 1U) {
            selected_board = nullptr;
            facts.board_selection_ambiguous = true;
        }
    }
    if (selected_board != nullptr) {
        facts.has_board_manufacturer = selected_board->has_manufacturer;
        facts.board_manufacturer_value = selected_board->manufacturer;
        facts.has_board_product_name = selected_board->has_product_name;
        facts.board_product_name_value = selected_board->product_name;
        facts.has_board_version = selected_board->has_version;
        facts.board_version_value = selected_board->version;
    }

    const chassis_candidate* selected_chassis = nullptr;
    if (chassis_records.size() == 1U) {
        selected_chassis = &chassis_records.front();
    } else if (!chassis_records.empty() && selected_board != nullptr &&
               selected_board->has_chassis_handle) {
        std::size_t matching = 0U;
        for (const chassis_candidate& chassis : chassis_records) {
            if (chassis.handle == selected_board->chassis_handle) {
                selected_chassis = &chassis;
                ++matching;
            }
        }
        if (matching != 1U) { selected_chassis = nullptr; }
    }
    if (selected_chassis != nullptr) {
        facts.has_chassis_type = true;
        facts.chassis_type_value = selected_chassis->type;
    } else if (chassis_records.size() > 1U) {
        facts.chassis_selection_ambiguous = true;
    }
    return facts;
}

/// Reads one raw SMBIOS table through an injectable two-stage sizing call.
template <typename Query>
inline result<std::vector<std::uint8_t>> fetch_table_with(
    const Query& query) {
    const ::UINT initial_required = query(nullptr, 0U);
    if (initial_required == 0U) { return fail(last_error()); }
    if (static_cast<std::size_t>(initial_required) > maximum_table_size) {
        return fail(errc::value_too_large);
    }
    std::vector<std::uint8_t> buffer(
        static_cast<std::size_t>(initial_required));
    constexpr unsigned int maximum_attempts = 3U;
    for (unsigned int attempt = 0U; attempt < maximum_attempts; ++attempt) {
        const ::UINT capacity = static_cast<::UINT>(buffer.size());
        const ::UINT written = query(buffer.data(), capacity);
        if (written == 0U) { return fail(last_error()); }
        if (static_cast<std::size_t>(written) > maximum_table_size) {
            return fail(errc::value_too_large);
        }
        if (static_cast<std::size_t>(written) > buffer.size()) {
            buffer.resize(static_cast<std::size_t>(written));
            continue;
        }
        buffer.resize(static_cast<std::size_t>(written));
        if (buffer.size() <= raw_smbios_header_size) {
            return fail(errc::malformed_data);
        }
        return buffer;
    }
    return fail(errc::temporarily_unavailable);
}

/// Reads one raw SMBIOS table through the documented Windows provider.
inline result<std::vector<std::uint8_t>> fetch_table() {
    const auto query = [](void* buffer, ::UINT capacity) {
        return ::GetSystemFirmwareTable(
            smbios_provider, 0U, buffer, capacity);
    };
    return fetch_table_with(query);
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
        table->data() + raw_smbios_header_size, header.length,
        header.major_version, header.minor_version);
}

/// Reduces one optional text fact into a query answer.
inline result<std::string> interpret_text(bool present,
                                          const std::string& value) {
    if (!present) { return fail(errc::not_found); }
    return value;
}

inline result<hardware_common::chassis_classification> interpret_chassis(
    const identity_facts& facts) {
    if (facts.chassis_selection_ambiguous) {
        return fail(errc::not_supported);
    }
    if (!facts.has_chassis_type) { return fail(errc::not_found); }
    return hardware_common::classify_chassis(facts.chassis_type_value);
}

inline result<std::string> interpret_board_text(
    const identity_facts& facts, bool present, const std::string& value) {
    if (facts.board_selection_ambiguous) {
        return fail(errc::not_supported);
    }
    return interpret_text(present, value);
}

/// Renders the recorded UUID as canonical lowercase text.
///
/// Since SMBIOS 2.6 the first three RFC 4122 fields are transmitted
/// little-endian, so they are reassembled for those versions before
/// rendering. Older revisions predate that clarification and retain the
/// recorded byte order used by established SMBIOS consumers. The remaining
/// octets always keep their recorded order. Both absence renderings the
/// specification defines report not_found rather than a value that
/// distinguishes nothing.
inline result<std::string> interpret_uuid(const identity_facts& facts) {
    if (!facts.has_uuid) { return fail(errc::not_found); }
    if (hardware_common::uuid_records_no_identifier(facts.uuid)) {
        return fail(errc::not_found);
    }
    const std::uint8_t* bytes = facts.uuid.value;
    const bool little_endian_fields = facts.major_version > 2U ||
        (facts.major_version == 2U && facts.minor_version >= 6U);
    const std::uint32_t time_low = little_endian_fields
        ? static_cast<std::uint32_t>(bytes[0]) |
              (static_cast<std::uint32_t>(bytes[1]) << 8U) |
              (static_cast<std::uint32_t>(bytes[2]) << 16U) |
              (static_cast<std::uint32_t>(bytes[3]) << 24U)
        : (static_cast<std::uint32_t>(bytes[0]) << 24U) |
              (static_cast<std::uint32_t>(bytes[1]) << 16U) |
              (static_cast<std::uint32_t>(bytes[2]) << 8U) |
              static_cast<std::uint32_t>(bytes[3]);
    const std::uint16_t time_mid = static_cast<std::uint16_t>(
        little_endian_fields
            ? static_cast<std::uint32_t>(bytes[4]) |
                  (static_cast<std::uint32_t>(bytes[5]) << 8U)
            : (static_cast<std::uint32_t>(bytes[4]) << 8U) |
                  static_cast<std::uint32_t>(bytes[5]));
    const std::uint16_t time_hi_and_version = static_cast<std::uint16_t>(
        little_endian_fields
            ? static_cast<std::uint32_t>(bytes[6]) |
                  (static_cast<std::uint32_t>(bytes[7]) << 8U)
            : (static_cast<std::uint32_t>(bytes[6]) << 8U) |
                  static_cast<std::uint32_t>(bytes[7]));
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
    return interpret_board_text(*facts, facts->has_board_manufacturer,
                                facts->board_manufacturer_value);
}

inline result<std::string> motherboard_product_name() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_board_text(*facts, facts->has_board_product_name,
                                facts->board_product_name_value);
}

inline result<std::string> motherboard_version() {
    const result<identity_facts> facts = collect_identity_facts();
    if (!facts) { return fail(facts.error()); }
    return interpret_board_text(*facts, facts->has_board_version,
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

inline result<std::string> wide_to_utf8(std::wstring_view value) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t),
                  "The Windows backend requires 16-bit wchar_t");
    std::u16string converted;
    converted.reserve(value.size());
    for (wchar_t unit : value) {
        converted.push_back(static_cast<char16_t>(unit));
    }
    return utf16_to_utf8(converted);
}

class devinfo_guard {
public:
    explicit devinfo_guard(::HDEVINFO handle) noexcept : handle_(handle) {}
    devinfo_guard(const devinfo_guard&) = delete;
    devinfo_guard& operator=(const devinfo_guard&) = delete;
    ~devinfo_guard() {
        if (handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr) {
            ::SetupDiDestroyDeviceInfoList(handle_);
        }
    }
    bool valid() const noexcept {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
    }
    ::HDEVINFO get() const noexcept { return handle_; }

private:
    ::HDEVINFO handle_;
};

inline result<std::vector<::syscape::hardware::memory_device>> memory_devices() {
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
    return hardware_common::parse_smbios_memory_devices(
        table->data() + raw_smbios_header_size, header.length,
        header.major_version > 3U ||
                (header.major_version == 3U && header.minor_version >= 1U)
            ? ::syscape::hardware::memory_speed_unit::megatransfers_per_second
            : ::syscape::hardware::memory_speed_unit::megahertz);
}

inline std::optional<std::uint32_t> parse_hex_wide(std::wstring_view text) noexcept {
    if (text.empty() || text.size() > 8U) { return std::nullopt; }
    std::uint32_t val = 0U;
    for (wchar_t ch : text) {
        val <<= 4U;
        if (ch >= L'0' && ch <= L'9') {
            val |= static_cast<std::uint32_t>(ch - L'0');
        } else if (ch >= L'a' && ch <= L'f') {
            val |= static_cast<std::uint32_t>(ch - L'a' + 10);
        } else if (ch >= L'A' && ch <= L'F') {
            val |= static_cast<std::uint32_t>(ch - L'A' + 10);
        } else {
            return std::nullopt;
        }
    }
    return val;
}

inline wchar_t ascii_upper(wchar_t value) noexcept {
    return value >= L'a' && value <= L'z'
        ? static_cast<wchar_t>(value - L'a' + L'A')
        : value;
}

/// Finds one ASCII PnP identifier component without assuming its letter case.
inline std::size_t find_id_component(std::wstring_view text,
                                     std::wstring_view component) noexcept {
    if (component.size() > text.size()) { return std::wstring_view::npos; }
    const std::size_t last = text.size() - component.size();
    for (std::size_t offset = 0U; offset <= last; ++offset) {
        bool equal = true;
        for (std::size_t index = 0U; index < component.size(); ++index) {
            if (ascii_upper(text[offset + index]) !=
                ascii_upper(component[index])) {
                equal = false;
                break;
            }
        }
        if (equal) { return offset; }
    }
    return std::wstring_view::npos;
}

struct registry_property {
    ::DWORD type = REG_NONE;
    std::size_t value_size = 0U;
    std::vector<std::uint8_t> bytes;
};

inline result<std::optional<registry_property>> read_registry_property(
    ::HDEVINFO devices, ::SP_DEVINFO_DATA& device, ::DWORD property) {
    ::DWORD required = 0U;
    ::DWORD type = REG_NONE;
    if (!::SetupDiGetDeviceRegistryPropertyW(
            devices, &device, property, &type, nullptr, 0U, &required)) {
        const ::DWORD error = ::GetLastError();
        if (error == ERROR_INVALID_DATA || error == ERROR_FILE_NOT_FOUND) {
            return std::optional<registry_property>();
        }
        if (error != ERROR_INSUFFICIENT_BUFFER) {
            return fail(std::error_code(
                static_cast<int>(error), std::system_category()));
        }
    }
    if (required == 0U) { return fail(errc::malformed_data); }

    registry_property result_property;
    result_property.type = type;
    constexpr unsigned int maximum_attempts = 3U;
    constexpr ::DWORD maximum_property_size = 1024U * 1024U;
    for (unsigned int attempt = 0U; attempt < maximum_attempts; ++attempt) {
        const std::size_t padding = 2U * sizeof(wchar_t);
        if (required > maximum_property_size ||
            static_cast<std::size_t>(required) >
                (std::numeric_limits<std::size_t>::max)() - padding) {
            return fail(errc::value_too_large);
        }
        result_property.bytes.assign(
            static_cast<std::size_t>(required) + padding, 0U);
        ::DWORD next_required = required;
        if (::SetupDiGetDeviceRegistryPropertyW(
                devices, &device, property, &result_property.type,
                result_property.bytes.data(), required, &next_required)) {
            if (next_required == 0U || next_required > required) {
                return fail(errc::malformed_data);
            }
            result_property.value_size = static_cast<std::size_t>(next_required);
            result_property.bytes.resize(
                static_cast<std::size_t>(required) + padding);
            return std::optional<registry_property>(std::move(result_property));
        }
        const ::DWORD error = ::GetLastError();
        if (error != ERROR_INSUFFICIENT_BUFFER || next_required <= required) {
            return fail(std::error_code(
                static_cast<int>(error), std::system_category()));
        }
        required = next_required;
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::optional<std::wstring>> property_wide_text(
    const std::optional<registry_property>& property, ::DWORD expected_type) {
    if (!property) { return std::optional<std::wstring>(); }
    if (property->type != expected_type ||
        property->value_size < sizeof(wchar_t) ||
        property->value_size % sizeof(wchar_t) != 0U) {
        return fail(errc::malformed_data);
    }
    const std::size_t units = property->value_size / sizeof(wchar_t);
    std::wstring text(units, L'\0');
    std::memcpy(text.data(), property->bytes.data(), property->value_size);
    if (text.back() != L'\0') { return fail(errc::malformed_data); }
    text.pop_back();
    return std::optional<std::wstring>(std::move(text));
}

inline result<std::optional<std::vector<std::wstring>>> property_wide_multisz(
    const std::optional<registry_property>& property) {
    if (!property) { return std::optional<std::vector<std::wstring>>(); }
    if (property->type != REG_MULTI_SZ ||
        property->value_size < 2U * sizeof(wchar_t) ||
        property->value_size % sizeof(wchar_t) != 0U) {
        return fail(errc::malformed_data);
    }
    const std::size_t units = property->value_size / sizeof(wchar_t);
    std::wstring storage(units, L'\0');
    std::memcpy(storage.data(), property->bytes.data(), property->value_size);
    if (storage[units - 1U] != L'\0' || storage[units - 2U] != L'\0') {
        return fail(errc::malformed_data);
    }
    std::vector<std::wstring> strings;
    std::size_t cursor = 0U;
    while (cursor + 1U < units && storage[cursor] != L'\0') {
        const std::size_t end = storage.find(L'\0', cursor);
        if (end == std::wstring::npos || end >= units - 1U) {
            return fail(errc::malformed_data);
        }
        strings.emplace_back(storage.substr(cursor, end - cursor));
        cursor = end + 1U;
    }
    return std::optional<std::vector<std::wstring>>(std::move(strings));
}

inline result<std::optional<::DWORD>> property_dword(
    const std::optional<registry_property>& property) {
    if (!property) { return std::optional<::DWORD>(); }
    if (property->type != REG_DWORD ||
        property->value_size != sizeof(::DWORD)) {
        return fail(errc::malformed_data);
    }
    ::DWORD value = 0U;
    std::memcpy(&value, property->bytes.data(), sizeof(value));
    return std::optional<::DWORD>(value);
}

/// Parses PCI hardware IDs into one device and reports whether it is a
/// representable terminal PCI function. Enumerator-only nodes legitimately
/// omit VEN_/DEV_ and are skipped by the caller.
inline result<bool> parse_pci_hardware_ids(
    const std::vector<std::wstring>& hardware_ids,
    ::syscape::hardware::pci_device& dev) {
    bool has_vendor = false;
    bool has_device = false;
    for (const std::wstring& hardware_id : hardware_ids) {
        const std::wstring_view hw_id(hardware_id);
        const std::size_t ven_pos = hw_id.find(L"VEN_");
        const std::size_t dev_pos = hw_id.find(L"DEV_");
        if (ven_pos != std::wstring_view::npos &&
            ven_pos + 8U <= hw_id.size()) {
            const auto value = parse_hex_wide(hw_id.substr(ven_pos + 4U, 4U));
            if (!value) { return fail(errc::malformed_data); }
            dev.vendor_id = static_cast<std::uint16_t>(*value);
            has_vendor = true;
        }
        if (dev_pos != std::wstring_view::npos &&
            dev_pos + 8U <= hw_id.size()) {
            const auto value = parse_hex_wide(hw_id.substr(dev_pos + 4U, 4U));
            if (!value) { return fail(errc::malformed_data); }
            dev.device_id = static_cast<std::uint16_t>(*value);
            has_device = true;
        }
        const std::size_t subsys_pos = hw_id.find(L"SUBSYS_");
        if (subsys_pos != std::wstring_view::npos &&
            subsys_pos + 15U <= hw_id.size()) {
            const auto subsystem_device =
                parse_hex_wide(hw_id.substr(subsys_pos + 7U, 4U));
            const auto subsystem_vendor =
                parse_hex_wide(hw_id.substr(subsys_pos + 11U, 4U));
            if (!subsystem_device || !subsystem_vendor) {
                return fail(errc::malformed_data);
            }
            dev.subsystem_device_id =
                static_cast<std::uint16_t>(*subsystem_device);
            dev.subsystem_vendor_id =
                static_cast<std::uint16_t>(*subsystem_vendor);
        }
        const std::size_t class_pos = hw_id.find(L"CC_");
        if (class_pos != std::wstring_view::npos &&
            class_pos + 9U <= hw_id.size()) {
            const auto base =
                parse_hex_wide(hw_id.substr(class_pos + 3U, 2U));
            const auto subclass =
                parse_hex_wide(hw_id.substr(class_pos + 5U, 2U));
            const auto interface_code =
                parse_hex_wide(hw_id.substr(class_pos + 7U, 2U));
            if (!base || !subclass || !interface_code) {
                return fail(errc::malformed_data);
            }
            dev.class_code = static_cast<std::uint8_t>(*base);
            dev.subclass_code = static_cast<std::uint8_t>(*subclass);
            dev.programming_interface =
                static_cast<std::uint8_t>(*interface_code);
            dev.device_class =
                hardware_common::classify_pci_class(*dev.class_code);
        }
    }
    return has_vendor && has_device;
}

/// Parses one physical USB device's hardware IDs. Composite-interface PDOs
/// carry an MI_ component and do not represent another bus device.
inline result<bool> parse_usb_hardware_ids(
    const std::vector<std::wstring>& hardware_ids,
    ::syscape::hardware::usb_device& dev) {
    for (const std::wstring& hardware_id : hardware_ids) {
        if (find_id_component(hardware_id, L"&MI_") !=
            std::wstring_view::npos) {
            return false;
        }
    }

    bool has_vendor = false;
    bool has_product = false;
    for (const std::wstring& hardware_id : hardware_ids) {
        const std::wstring_view id(hardware_id);
        const std::size_t vendor = find_id_component(id, L"VID_");
        if (vendor != std::wstring_view::npos) {
            if (vendor + 8U > id.size()) { return fail(errc::malformed_data); }
            const auto value = parse_hex_wide(id.substr(vendor + 4U, 4U));
            if (!value) { return fail(errc::malformed_data); }
            dev.vendor_id = static_cast<std::uint16_t>(*value);
            has_vendor = true;
        }
        const std::size_t product = find_id_component(id, L"PID_");
        if (product != std::wstring_view::npos) {
            if (product + 8U > id.size()) { return fail(errc::malformed_data); }
            const auto value = parse_hex_wide(id.substr(product + 4U, 4U));
            if (!value) { return fail(errc::malformed_data); }
            dev.product_id = static_cast<std::uint16_t>(*value);
            has_product = true;
        }
        const std::size_t revision = find_id_component(id, L"REV_");
        if (revision != std::wstring_view::npos) {
            if (revision + 8U > id.size()) { return fail(errc::malformed_data); }
            const auto value = parse_hex_wide(id.substr(revision + 4U, 4U));
            if (!value) { return fail(errc::malformed_data); }
            dev.bcd_device = static_cast<std::uint16_t>(*value);
        }
    }
    return has_vendor && has_product;
}

/// Parses descriptor-level class codes from USB compatible IDs. Windows
/// records these in SPDRP_COMPATIBLEIDS rather than SPDRP_HARDWAREID.
inline result<void> parse_usb_compatible_ids(
    const std::vector<std::wstring>& compatible_ids,
    ::syscape::hardware::usb_device& dev) {
    for (const std::wstring& compatible_id : compatible_ids) {
        const std::wstring_view id(compatible_id);
        const std::size_t class_position = find_id_component(id, L"CLASS_");
        if (!dev.device_class && class_position != std::wstring_view::npos) {
            if (class_position + 8U > id.size()) {
                return fail(errc::malformed_data);
            }
            const auto value =
                parse_hex_wide(id.substr(class_position + 6U, 2U));
            if (!value) { return fail(errc::malformed_data); }
            dev.device_class = static_cast<std::uint8_t>(*value);
        }
        const std::size_t subclass_position =
            find_id_component(id, L"SUBCLASS_");
        if (!dev.device_subclass &&
            subclass_position != std::wstring_view::npos) {
            if (subclass_position + 11U > id.size()) {
                return fail(errc::malformed_data);
            }
            const auto value =
                parse_hex_wide(id.substr(subclass_position + 9U, 2U));
            if (!value) { return fail(errc::malformed_data); }
            dev.device_subclass = static_cast<std::uint8_t>(*value);
        }
        const std::size_t protocol_position =
            find_id_component(id, L"PROT_");
        if (!dev.device_protocol &&
            protocol_position != std::wstring_view::npos) {
            if (protocol_position + 7U > id.size()) {
                return fail(errc::malformed_data);
            }
            const auto value =
                parse_hex_wide(id.substr(protocol_position + 5U, 2U));
            if (!value) { return fail(errc::malformed_data); }
            dev.device_protocol = static_cast<std::uint8_t>(*value);
        }
    }
    return {};
}

inline result<std::vector<::syscape::hardware::pci_device>> pci_devices() {
    const devinfo_guard dev_info(::SetupDiGetClassDevsW(
        nullptr, L"PCI", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES));
    if (!dev_info.valid()) {
        return fail(last_error());
    }

    std::vector<::syscape::hardware::pci_device> result_devices;
    for (::DWORD index = 0U;; ++index) {
        ::SP_DEVINFO_DATA dev_data{};
        dev_data.cbSize = sizeof(::SP_DEVINFO_DATA);
        if (!::SetupDiEnumDeviceInfo(dev_info.get(), index, &dev_data)) {
            const ::DWORD error = ::GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) { break; }
            return fail(std::error_code(
                static_cast<int>(error), std::system_category()));
        }
        ::syscape::hardware::pci_device dev;

        // Query Hardware IDs
        const auto hw_property = read_registry_property(
            dev_info.get(), dev_data, SPDRP_HARDWAREID);
        if (!hw_property) { return fail(hw_property.error()); }
        const auto hardware_ids = property_wide_multisz(*hw_property);
        if (!hardware_ids) { return fail(hardware_ids.error()); }
        if (!*hardware_ids) { continue; }
        const result<bool> terminal = parse_pci_hardware_ids(**hardware_ids, dev);
        if (!terminal) { return fail(terminal.error()); }
        if (!*terminal) { continue; }

        // Query Service (Driver)
        const auto svc_property = read_registry_property(
            dev_info.get(), dev_data, SPDRP_SERVICE);
        if (!svc_property) { return fail(svc_property.error()); }
        const auto svc_text = property_wide_text(*svc_property, REG_SZ);
        if (!svc_text) { return fail(svc_text.error()); }
        if (*svc_text) {
            const auto utf8_svc = wide_to_utf8(**svc_text);
            if (!utf8_svc) { return fail(utf8_svc.error()); }
            if (!utf8_svc->empty()) { dev.driver = *utf8_svc; }
        }

        // Query Location Information (Slot Name / BDF)
        const auto loc_property = read_registry_property(
            dev_info.get(), dev_data, SPDRP_LOCATION_INFORMATION);
        if (!loc_property) { return fail(loc_property.error()); }
        const auto loc_text = property_wide_text(*loc_property, REG_SZ);
        if (!loc_text) { return fail(loc_text.error()); }
        if (*loc_text) {
            const auto utf8_loc = wide_to_utf8(**loc_text);
            if (!utf8_loc) { return fail(utf8_loc.error()); }
            if (!utf8_loc->empty()) { dev.slot_name = *utf8_loc; }
        }

        // Query Bus Number
        const auto bus_property = read_registry_property(
            dev_info.get(), dev_data, SPDRP_BUSNUMBER);
        if (!bus_property) { return fail(bus_property.error()); }
        const auto bus_number = property_dword(*bus_property);
        if (!bus_number) { return fail(bus_number.error()); }
        if (*bus_number) {
            if (**bus_number > 0xFFU) { return fail(errc::malformed_data); }
            dev.bus = static_cast<std::uint8_t>(**bus_number);
        }

        result_devices.push_back(std::move(dev));
    }

    std::sort(result_devices.begin(), result_devices.end(), hardware_common::compare_pci_devices);
    return result_devices;
}

inline result<std::vector<::syscape::hardware::usb_device>> usb_devices() {
    const devinfo_guard dev_info(::SetupDiGetClassDevsW(
        nullptr, L"USB", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES));
    if (!dev_info.valid()) {
        return fail(last_error());
    }

    std::vector<::syscape::hardware::usb_device> result_devices;
    for (::DWORD index = 0U;; ++index) {
        ::SP_DEVINFO_DATA dev_data{};
        dev_data.cbSize = sizeof(::SP_DEVINFO_DATA);
        if (!::SetupDiEnumDeviceInfo(dev_info.get(), index, &dev_data)) {
            const ::DWORD error = ::GetLastError();
            if (error == ERROR_NO_MORE_ITEMS) { break; }
            return fail(std::error_code(
                static_cast<int>(error), std::system_category()));
        }
        ::syscape::hardware::usb_device dev;

        // Query Hardware IDs
        const auto hw_property = read_registry_property(
            dev_info.get(), dev_data, SPDRP_HARDWAREID);
        if (!hw_property) { return fail(hw_property.error()); }
        const auto hardware_ids = property_wide_multisz(*hw_property);
        if (!hardware_ids) { return fail(hardware_ids.error()); }
        if (!*hardware_ids) { continue; }
        const result<bool> physical =
            parse_usb_hardware_ids(**hardware_ids, dev);
        if (!physical) { return fail(physical.error()); }
        if (!*physical) { continue; }

        const auto compatible_property = read_registry_property(
            dev_info.get(), dev_data, SPDRP_COMPATIBLEIDS);
        if (!compatible_property) { return fail(compatible_property.error()); }
        const auto compatible_ids =
            property_wide_multisz(*compatible_property);
        if (!compatible_ids) { return fail(compatible_ids.error()); }
        if (*compatible_ids) {
            const result<void> parsed =
                parse_usb_compatible_ids(**compatible_ids, dev);
            if (!parsed) { return fail(parsed.error()); }
        }

        // SetupAPI manufacturer, friendly-name, and description properties
        // come from the installed INF and are not USB string descriptors.

        const auto bus_property = read_registry_property(
            dev_info.get(), dev_data, SPDRP_BUSNUMBER);
        if (!bus_property) { return fail(bus_property.error()); }
        const auto bus_number = property_dword(*bus_property);
        if (!bus_number) { return fail(bus_number.error()); }
        if (*bus_number) {
            if (**bus_number > 0xFFU) { return fail(errc::malformed_data); }
            dev.bus_number = static_cast<std::uint8_t>(**bus_number);
        }

        result_devices.push_back(std::move(dev));
    }

    std::sort(result_devices.begin(), result_devices.end(), hardware_common::compare_usb_devices);
    return result_devices;
}

} // namespace hardware_backend
} // namespace detail
} // namespace syscape

#endif
