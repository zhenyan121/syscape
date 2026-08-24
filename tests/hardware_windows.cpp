#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <windows.h>

#include <syscape/hardware.hpp>
#include <syscape/detail/hardware/common.hpp>
#include <syscape/detail/hardware/windows.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

/// One fabricated SMBIOS structure.
struct structure_spec {
    /// Complete formatted area including the four-byte header whose second
    /// byte the builder patches to the recorded length.
    std::vector<std::uint8_t> formatted;
    /// Indexed strings, where index one is strings[0].
    std::vector<std::string> strings;
};

/// Builds one raw SMBIOS table exactly as the interface documents its
/// layout: formatted area, indexed null-terminated strings, double-null
/// terminator.
std::vector<std::uint8_t> make_table(
    const std::vector<structure_spec>& structures,
    bool end_marker = true) {
    std::vector<std::uint8_t> blob;
    for (const structure_spec& structure : structures) {
        expect(structure.formatted.size() >= 4U &&
                   structure.formatted.size() <= 255U,
               "Synthetic formatted areas must fit the documented length "
               "byte");
        std::vector<std::uint8_t> formatted = structure.formatted;
        formatted[1] = static_cast<std::uint8_t>(formatted.size());
        blob.insert(blob.end(), formatted.begin(), formatted.end());
        for (const std::string& text : structure.strings) {
            blob.insert(blob.end(), text.begin(), text.end());
            blob.push_back(0U);
        }
        if (structure.strings.empty()) { blob.push_back(0U); }
        blob.push_back(0U);
    }
    if (end_marker) {
        blob.push_back(127U);
        blob.push_back(4U);
        blob.push_back(0xFEU);
        blob.push_back(0xFFU);
        blob.push_back(0U);
        blob.push_back(0U);
    }
    return blob;
}

std::vector<std::uint8_t> firmware_structure(std::uint8_t vendor_index,
                                             std::uint8_t version_index,
                                             std::uint8_t date_index) {
    return {
        0U, 0U, 0U, 0U,
        vendor_index, version_index, 0U, 0U,
        date_index, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
}

std::vector<std::uint8_t> system_structure(std::uint8_t manufacturer_index,
                                           std::uint8_t product_index,
                                           std::uint8_t version_index) {
    return {
        1U, 0U, 0U, 4U,
        manufacturer_index, product_index, version_index, 0U};
}

/// Extends a system structure with the sixteen-byte UUID field.
std::vector<std::uint8_t> with_uuid(std::vector<std::uint8_t> formatted,
                                    const std::uint8_t (&uuid)[16]) {
    formatted.resize(0x18U, 0U);
    for (std::size_t index = 0; index < 16U; ++index) {
        formatted[8U + index] = uuid[index];
    }
    return formatted;
}

std::vector<std::uint8_t> board_structure(std::uint8_t manufacturer_index,
                                          std::uint8_t product_index,
                                          std::uint8_t version_index) {
    return {
        2U, 0U, 0U, 4U,
        manufacturer_index, product_index, version_index, 0U};
}

std::vector<std::uint8_t> chassis_structure(std::uint8_t type_byte) {
    return {
        3U, 0U, 0U, 4U,
        1U, type_byte, 0U, 0U, 0U};
}

std::vector<std::uint8_t> identified_board_structure(
    std::uint16_t handle, std::uint16_t chassis_handle,
    bool hosting_board, std::uint8_t board_type) {
    return {
        2U, 0U,
        static_cast<std::uint8_t>(handle & 0xFFU),
        static_cast<std::uint8_t>(handle >> 8U),
        1U, 2U, 3U, 0U, 0U,
        static_cast<std::uint8_t>(hosting_board ? 1U : 0U),
        0U,
        static_cast<std::uint8_t>(chassis_handle & 0xFFU),
        static_cast<std::uint8_t>(chassis_handle >> 8U),
        board_type,
        0U};
}

std::vector<std::uint8_t> identified_chassis_structure(
    std::uint16_t handle, std::uint8_t type_byte) {
    return {
        3U, 0U,
        static_cast<std::uint8_t>(handle & 0xFFU),
        static_cast<std::uint8_t>(handle >> 8U),
        1U, type_byte, 0U, 0U, 0U};
}

void test_full_table_parse() {
    namespace backend = syscape::detail::hardware_backend;

    const std::uint8_t uuid[16] = {
        0x00U, 0x02U, 0x00U, 0x03U,
        0x04U, 0x05U,
        0x06U, 0x07U,
        0x08U, 0x09U, 0x0AU, 0x0BU, 0x0CU, 0x0DU, 0x0EU, 0x0FU};
    const std::vector<std::uint8_t> table = make_table({
        {firmware_structure(1U, 2U, 3U),
         {"Firmware Vendor", "FW 1.0", "01/02/2024"}},
        {with_uuid(system_structure(1U, 2U, 3U), uuid),
         {"System Vendor", "Product X", "Rev A"}},
        {board_structure(1U, 2U, 0U), {"Board Vendor", "Board Z"}},
        {chassis_structure(static_cast<std::uint8_t>(9U | 0x80U)),
         {"Enclosure Maker"}},
    });

    const syscape::result<backend::identity_facts> facts =
        backend::parse_smbios_table(table.data(), table.size(), 2U, 6U);
    expect(facts.has_value(),
           "A conforming table must parse into plain facts");
    if (!facts) { return; }

    expect(facts->has_firmware_vendor &&
               facts->firmware_vendor_value == "Firmware Vendor",
           "The BIOS vendor string must come from its recorded index");
    expect(facts->has_firmware_version &&
               facts->firmware_version_value == "FW 1.0",
           "The BIOS version string must come from its recorded index");
    expect(facts->has_firmware_release_date &&
               facts->firmware_release_date_value == "01/02/2024",
           "The BIOS release date must come from its recorded index");

    expect(facts->has_system_manufacturer &&
               facts->system_manufacturer_value == "System Vendor",
           "The system manufacturer must come from its recorded index");
    expect(facts->has_system_product_name &&
               facts->system_product_name_value == "Product X",
           "The product name must come from its recorded index");
    expect(facts->has_system_product_version &&
               facts->system_product_version_value == "Rev A",
           "The system version must come from its recorded index");

    expect(facts->has_uuid,
           "A full-length system record must carry the UUID field");

    expect(facts->has_board_manufacturer &&
               facts->board_manufacturer_value == "Board Vendor",
           "The board manufacturer must come from its recorded index");
    expect(facts->has_board_product_name &&
               facts->board_product_name_value == "Board Z",
           "The board product name must come from its recorded index");
    expect(!facts->has_board_version,
           "An index of zero records an absent board version");

    expect(facts->has_chassis_type && facts->chassis_type_value == 9U,
           "The enclosure classification must keep only the documented "
           "low seven bits");

    const syscape::result<std::string> rendered =
        backend::interpret_uuid(*facts);
    expect(rendered.has_value() &&
               *rendered == "03000200-0504-0706-0809-0a0b0c0d0e0f",
           "The first three RFC 4122 fields are transmitted little-endian "
           "and must render in presentation order");
}

void test_legacy_uuid_byte_order() {
    namespace backend = syscape::detail::hardware_backend;

    const std::uint8_t uuid[16] = {
        0x00U, 0x11U, 0x22U, 0x33U,
        0x44U, 0x55U,
        0x66U, 0x77U,
        0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU};
    const std::vector<std::uint8_t> table = make_table({
        {with_uuid(system_structure(1U, 2U, 0U), uuid),
         {"Vendor", "Product"}},
    });
    const syscape::result<backend::identity_facts> facts =
        backend::parse_smbios_table(table.data(), table.size(), 2U, 5U);
    expect(facts.has_value(), "A legacy SMBIOS table must parse");
    if (!facts) { return; }
    const syscape::result<std::string> rendered =
        backend::interpret_uuid(*facts);
    expect(rendered.has_value() &&
               *rendered == "00112233-4455-6677-8899-aabbccddeeff",
           "Pre-2.6 UUID fields must retain their recorded byte order");
}

void test_table_buffer_growth() {
    namespace backend = syscape::detail::hardware_backend;

    unsigned int calls = 0U;
    const auto grows_once = [&calls](void*, ::UINT capacity) -> ::UINT {
        ++calls;
        if (capacity == 0U) { return 12U; }
        if (capacity == 12U) { return 16U; }
        return 16U;
    };
    const syscape::result<std::vector<std::uint8_t>> grown =
        backend::fetch_table_with(grows_once);
    expect(grown.has_value() && grown->size() == 16U && calls == 3U,
           "A table that grows after sizing must be retried");

    const auto keeps_growing = [](void*, ::UINT capacity) -> ::UINT {
        return capacity == 0U ? 9U : capacity + 1U;
    };
    const syscape::result<std::vector<std::uint8_t>> unstable =
        backend::fetch_table_with(keeps_growing);
    expect(unstable.error() == syscape::errc::temporarily_unavailable,
           "A table that keeps growing through the retry cap must be "
           "temporarily unavailable");
}

void test_interpretation() {
    namespace backend = syscape::detail::hardware_backend;

    backend::identity_facts facts;
    facts.has_chassis_type = true;
    facts.chassis_type_value = 31U;
    const auto convertible = backend::interpret_chassis(facts);
    expect(convertible.has_value() &&
               *convertible == syscape::detail::hardware_common::
                                   chassis_classification::convertible,
           "A recorded classification byte must classify through the shared "
           "vocabulary");

    facts.chassis_type_value = 200U;
    expect(!backend::interpret_chassis(facts),
           "A classification outside the documented table is malformed");

    facts.chassis_type_value = 3U;
    facts.has_chassis_type = false;
    expect(backend::interpret_chassis(facts).error() ==
               syscape::errc::not_found,
           "An enclosure record without its classification reports "
           "not_found");

    const syscape::result<std::string> absent =
        backend::interpret_text(false, std::string("ignored"));
    expect(absent.error() == syscape::errc::not_found,
           "Absent identity facts report not_found instead of emptiness");
}

void test_duplicate_singleton_records_fail() {
    namespace backend = syscape::detail::hardware_backend;

    const std::vector<std::uint8_t> table = make_table({
        {system_structure(1U, 2U, 0U), {"First Vendor", "First Product"}},
        {system_structure(1U, 2U, 0U), {"Second Vendor", "Second Product"}},
    });
    const syscape::result<backend::identity_facts> facts =
        backend::parse_smbios_table(table.data(), table.size());
    expect(!facts && facts.error() == syscape::errc::malformed_data,
           "A table cannot contain duplicate System Information records");

    const std::vector<std::uint8_t> duplicate_bios = make_table({
        {firmware_structure(1U, 2U, 3U), {"V", "R", "D"}},
        {firmware_structure(1U, 2U, 3U), {"V", "R", "D"}},
    });
    expect(backend::parse_smbios_table(duplicate_bios.data(),
                                       duplicate_bios.size())
                   .error() == syscape::errc::malformed_data,
           "A table cannot contain duplicate BIOS Information records");
}

void test_primary_board_and_chassis_selection() {
    namespace backend = syscape::detail::hardware_backend;

    const std::vector<std::uint8_t> table = make_table({
        {identified_board_structure(0x1000U, 0x2000U, false, 0x09U),
         {"Addon Vendor", "Daughterboard", "A"}},
        {identified_chassis_structure(0x2000U, 21U), {"Peripheral"}},
        {identified_board_structure(0x1001U, 0x2001U, true, 0x0AU),
         {"Main Vendor", "Motherboard", "B"}},
        {identified_chassis_structure(0x2001U, 23U), {"Main enclosure"}},
    });
    const auto facts = backend::parse_smbios_table(
        table.data(), table.size(), 3U, 0U);
    expect(facts.has_value(), "A linked multi-board table must parse");
    if (!facts) { return; }
    expect(facts->has_board_product_name &&
               facts->board_product_name_value == "Motherboard",
           "The hosting motherboard must win over an earlier daughterboard");
    expect(facts->has_chassis_type && facts->chassis_type_value == 23U,
           "The motherboard's chassis handle must select the main enclosure");

    const std::vector<std::uint8_t> ambiguous = make_table({
        {identified_board_structure(1U, 10U, false, 0x09U),
         {"A", "Board A", "1"}},
        {identified_board_structure(2U, 11U, false, 0x09U),
         {"B", "Board B", "2"}},
    });
    const auto ambiguous_facts = backend::parse_smbios_table(
        ambiguous.data(), ambiguous.size(), 3U, 0U);
    expect(ambiguous_facts.has_value() &&
               ambiguous_facts->board_selection_ambiguous,
           "Multiple boards without one motherboard must stay ambiguous");
    if (ambiguous_facts) {
        expect(backend::interpret_board_text(
                   *ambiguous_facts,
                   ambiguous_facts->has_board_product_name,
                   ambiguous_facts->board_product_name_value)
                       .error() == syscape::errc::not_supported,
               "An ambiguous board selection must not guess one record");
    }

    const std::vector<std::uint8_t> ambiguous_chassis = make_table({
        {identified_chassis_structure(20U, 21U), {"Peripheral"}},
        {identified_chassis_structure(21U, 23U), {"Main"}},
    });
    const auto ambiguous_chassis_facts = backend::parse_smbios_table(
        ambiguous_chassis.data(), ambiguous_chassis.size(), 3U, 0U);
    expect(ambiguous_chassis_facts.has_value() &&
               backend::interpret_chassis(*ambiguous_chassis_facts).error() ==
                   syscape::errc::not_supported,
           "Multiple unassociated enclosures must not guess by record order");
}

void test_string_extraction() {
    namespace backend = syscape::detail::hardware_backend;

    const std::uint8_t payload[] = {
        1U, 8U, 0U, 4U,
        1U, 2U, 0U, 0U,
        'V', 'E', 'N', 0U,
        0U};
    backend::structure_view view;
    view.formatted = payload;
    view.formatted_size = 8U;
    view.strings = payload + 8U;
    view.strings_size = 3U;

    const auto named =
        backend::extract_string(view, static_cast<std::uint8_t>(1U));
    expect(named.has_value() && named->present &&
               named->value == "VEN",
           "Index one must copy the first recorded string");

    const auto unnamed = backend::extract_string(
        view, static_cast<std::uint8_t>(0U));
    expect(unnamed.has_value() && !unnamed->present,
           "Index zero names no string and records an absent fact");

    const auto beyond = backend::extract_string(
        view, static_cast<std::uint8_t>(2U));
    expect(!beyond.has_value(),
           "An index beyond the recorded string count contradicts the "
           "table's own indexing");
}

void test_absent_and_empty_strings() {
    namespace backend = syscape::detail::hardware_backend;

    // A string body that is present but empty contributes no usable value,
    // so the fact records absence. Encoding it as the only string makes the
    // empty body directly reachable through index one.
    const std::vector<std::uint8_t> empty_table = make_table({
        {system_structure(1U, 0U, 0U), {""}},
    });
    const syscape::result<backend::identity_facts> empty_facts =
        backend::parse_smbios_table(empty_table.data(),
                                    empty_table.size());
    expect(empty_facts.has_value(), "An empty string body must parse");
    if (empty_facts) {
        expect(!empty_facts->has_system_manufacturer,
               "A present-but-empty string records an absent fact");
    }

    const std::uint8_t uuid_zero[16] = {};
    const std::vector<std::uint8_t> zero_table = make_table({
        {with_uuid(system_structure(1U, 2U, 0U), uuid_zero),
         {"Vendor", "Product"}},
    });
    const syscape::result<backend::identity_facts> zero_facts =
        backend::parse_smbios_table(zero_table.data(),
                                    zero_table.size());
    expect(zero_facts.has_value(), "An all-zero UUID record must parse");
    if (zero_facts) {
        expect(!zero_facts->has_system_product_version,
               "Index zero names no string and records an absent fact");
        expect(zero_facts->has_uuid,
               "The UUID field itself is still present in the record");
        const syscape::result<std::string> answer =
            backend::interpret_uuid(*zero_facts);
        expect(answer.error() == syscape::errc::not_found,
               "The all-zero UUID is the specification's no-identifier "
               "rendering and reports not_found");
    }

    std::uint8_t uuid_ff[16];
    for (std::uint8_t& octet : uuid_ff) { octet = 0xFFU; }
    const std::vector<std::uint8_t> ff_table = make_table({
        {with_uuid(system_structure(1U, 2U, 3U), uuid_ff),
         {"Vendor", "Product", "Version"}},
    });
    const syscape::result<backend::identity_facts> ff_facts =
        backend::parse_smbios_table(ff_table.data(), ff_table.size());
    expect(ff_facts.has_value(), "An all-one UUID record must parse");
    if (ff_facts) {
        expect(backend::interpret_uuid(*ff_facts).error() ==
                   syscape::errc::not_found,
               "The all-one UUID is the other no-identifier rendering");
    }
}

void test_missing_uuid_field() {
    namespace backend = syscape::detail::hardware_backend;

    const std::vector<std::uint8_t> table = make_table({
        {system_structure(1U, 2U, 0U), {"Vendor", "Product"}},
    });
    const syscape::result<backend::identity_facts> facts =
        backend::parse_smbios_table(table.data(), table.size());
    expect(facts.has_value() && !facts->has_uuid,
           "A short system record predates the UUID field and records no "
           "identifier");
    if (facts) {
        expect(backend::interpret_uuid(*facts).error() ==
                   syscape::errc::not_found,
               "No recorded identifier reports not_found");
    }
}

void test_malformed_tables() {
    namespace backend = syscape::detail::hardware_backend;

    std::vector<std::uint8_t> unterminated = make_table(
        {{system_structure(1U, 2U, 0U), {"Vendor", "Product"}}}, false);
    unterminated.pop_back();
    expect(!backend::parse_smbios_table(unterminated.data(),
                                        unterminated.size()),
           "Every structure requires its double-null string terminator");

    const std::vector<std::uint8_t> truncated = make_table({
        {system_structure(1U, 2U, 0U), {"Vendor", "Product"}},
    });
    expect(!backend::parse_smbios_table(truncated.data(),
                                        truncated.size() - 5U),
           "A table cut inside a record's string terminator fails instead "
           "of reading out of bounds");

    std::vector<std::uint8_t> dangling_tail = make_table(
        {{system_structure(1U, 2U, 0U), {"Vendor", "Product"}}}, false);
    dangling_tail.push_back(0xABU);
    dangling_tail.push_back(0xCDU);
    expect(!backend::parse_smbios_table(dangling_tail.data(),
                                        dangling_tail.size()),
           "Trailing bytes shorter than one record header fail as malformed "
           "data");

    const std::vector<std::uint8_t> short_bios = make_table({
        {std::vector<std::uint8_t>{0U, 0U, 0U, 0U, 1U, 2U, 0U, 0U, 3U},
         {"V", "R", "D"}},
    });
    expect(!backend::parse_smbios_table(short_bios.data(),
                                        short_bios.size()),
           "A BIOS record shorter than its documented eighteen-byte floor "
           "is malformed platform data");

    std::vector<std::uint8_t> bad_length = make_table({
        {system_structure(1U, 2U, 0U), {"Vendor", "Product"}},
    });
    bad_length[1] = 200U;
    expect(!backend::parse_smbios_table(bad_length.data(),
                                        bad_length.size()),
           "A declared record length beyond the buffer is malformed data");

    std::vector<std::uint8_t> tiny_length = make_table({
        {system_structure(1U, 2U, 0U), {"Vendor", "Product"}},
    });
    tiny_length[1] = 3U;
    expect(!backend::parse_smbios_table(tiny_length.data(),
                                        tiny_length.size()),
           "A record cannot be smaller than its own four-byte header");

    const std::vector<std::uint8_t> foreign_index = make_table({
        {system_structure(9U, 2U, 0U), {"Vendor", "Product"}},
    });
    expect(!backend::parse_smbios_table(foreign_index.data(),
                                        foreign_index.size()),
           "An index beyond the recorded strings contradicts the "
           "specification's indexing");

    const std::vector<std::uint8_t> no_end_marker = make_table({
        {system_structure(1U, 2U, 0U), {"Vendor", "Product"}},
    }, false);
    expect(backend::parse_smbios_table(no_end_marker.data(),
                                       no_end_marker.size(), 3U, 0U)
                   .error() == syscape::errc::malformed_data,
           "SMBIOS 2.2 and later tables require an end-of-table record");
    expect(backend::parse_smbios_table(no_end_marker.data(),
                                       no_end_marker.size(), 2U, 1U)
               .has_value(),
           "A pre-2.2 table remains valid without the later end marker");

    const std::uint8_t short_end_marker[] = {127U, 0U, 0U, 0U, 0U, 0U};
    expect(backend::parse_smbios_table(short_end_marker,
                                       sizeof(short_end_marker), 3U, 0U)
                   .error() == syscape::errc::malformed_data,
           "An end-of-table record still requires a four-byte header");

    const std::uint8_t unterminated_end_marker[] = {127U, 4U, 0U, 0U};
    expect(backend::parse_smbios_table(
               unterminated_end_marker, sizeof(unterminated_end_marker),
               3U, 0U)
                   .error() == syscape::errc::malformed_data,
           "An end-of-table record requires its double-null terminator");

    const std::vector<std::uint8_t> short_multiple_boards = make_table({
        {board_structure(1U, 2U, 0U), {"A", "Board A"}},
        {board_structure(1U, 2U, 0U), {"B", "Board B"}},
    });
    expect(backend::parse_smbios_table(short_multiple_boards.data(),
                                       short_multiple_boards.size())
                   .error() == syscape::errc::malformed_data,
           "Multiple boards require their containment fields");
}

} // namespace

int main() {
    test_full_table_parse();
    test_legacy_uuid_byte_order();
    test_table_buffer_growth();
    test_interpretation();
    test_duplicate_singleton_records_fail();
    test_primary_board_and_chassis_selection();
    test_string_extraction();
    test_absent_and_empty_strings();
    test_missing_uuid_field();
    test_malformed_tables();
    return failures == 0 ? 0 : 1;
}
