#include <cerrno>
#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>

#include <syscape/hardware.hpp>
#include <syscape/detail/hardware/common.hpp>
#include <syscape/detail/hardware/linux.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_number_parser() {
    namespace backend = syscape::detail::hardware_backend;

    const auto typical = backend::parse_number("4\n");
    expect(typical && *typical == 4U,
           "A documented chassis-type rendering must parse as a decimal");

    const auto large = backend::parse_number("36");
    expect(large && *large == 36U,
           "The largest documented chassis type must fit the field");

    const auto trailing = backend::parse_number("9 kb\n");
    expect(!trailing &&
               trailing.error() == syscape::errc::malformed_data,
           "Trailing text after a number must be malformed platform data");

    const auto signed_value = backend::parse_number("-5\n");
    expect(!signed_value &&
               signed_value.error() == syscape::errc::malformed_data,
           "A signed rendering cannot describe this attribute");

    const auto word = backend::parse_number("desktop");
    expect(!word && word.error() == syscape::errc::malformed_data,
           "A nonnumeric rendering must be malformed platform data");

    const auto blank = backend::parse_number("   \n");
    expect(!blank && blank.error() == syscape::errc::malformed_data,
           "A blank rendering cannot describe this attribute");
}

void test_dmi_interface_probe() {
    namespace backend = syscape::detail::hardware_backend;

    const auto missing = [](const char*, struct ::stat*) {
        errno = ENOENT;
        return -1;
    };
    const auto absent = backend::dmi_interface_present_with(missing);
    expect(absent.has_value() && !*absent,
           "A missing DMI-id directory records an unsupported source");

    const auto denied = [](const char*, struct ::stat*) {
        errno = EACCES;
        return -1;
    };
    const auto permission = backend::dmi_interface_present_with(denied);
    expect(!permission &&
               permission.error() ==
                   std::error_code(EACCES, std::generic_category()),
           "A DMI-id probe must preserve its native permission failure");

    const auto io_failure = [](const char*, struct ::stat*) {
        errno = EIO;
        return -1;
    };
    const auto io = backend::dmi_interface_present_with(io_failure);
    expect(!io && io.error() ==
                      std::error_code(EIO, std::generic_category()),
           "A DMI-id probe must preserve native I/O failures");
}

void test_chassis_classifier() {
    namespace common = syscape::detail::hardware_common;
    using classification = syscape::detail::hardware_common::
        chassis_classification;

    const auto expect_maps = [](std::uint8_t recorded,
                                classification wanted) {
        const auto mapped = common::classify_chassis(recorded);
        return mapped.has_value() && *mapped == wanted;
    };

    expect(expect_maps(3U, classification::desktop),
           "SMBIOS enclosure type three must map onto desktop");
    expect(expect_maps(9U, classification::laptop),
           "SMBIOS enclosure type nine must map onto laptop");
    expect(expect_maps(10U, classification::notebook),
           "SMBIOS enclosure type ten must map onto notebook");
    expect(expect_maps(23U, classification::rack_mount_chassis),
           "SMBIOS enclosure type twenty-three must map onto rack mount");
    expect(expect_maps(35U, classification::mini_pc),
           "SMBIOS enclosure type thirty-five must map onto mini PC");
    expect(expect_maps(36U, classification::stick_pc),
           "The largest documented type must map without special cases");
    expect(expect_maps(2U, classification::unknown),
           "The documented no-classification rendering must stay unknown");
    expect(expect_maps(1U, classification::other),
           "The documented other rendering must stay other");

    expect(!common::classify_chassis(0U),
           "Type zero lies outside the documented one-based range");
    expect(!common::classify_chassis(37U),
           "Types beyond the documented table are malformed platform data");
}

void test_uuid_renderer() {
    namespace common = syscape::detail::hardware_common;

    const std::uint8_t tail[8] = {0x00U, 0x06U, 0x00U, 0x07U,
                                  0x00U, 0x08U, 0x00U, 0x09U};
    const std::string rendered =
        common::render_canonical_uuid(0x03000200U, 0x0400U, 0x0500U, tail);
    expect(rendered == "03000200-0400-0500-0006-000700080009",
           "The canonical rendering must place every RFC 4122 field and "
           "use lowercase hexadecimal digits");

    const std::uint8_t zero_tail[8] = {};
    expect(common::render_canonical_uuid(0U, 0U, 0U, zero_tail) ==
               "00000000-0000-0000-0000-000000000000",
           "An all-zero record renders before callers apply the absence "
           "rule");

    common::uuid_octets zeros{};
    expect(common::uuid_records_no_identifier(zeros),
           "The all-zero rendering records no identifier by specification");

    common::uuid_octets ones{};
    for (std::uint8_t& octet : ones.value) { octet = 0xFFU; }
    expect(common::uuid_records_no_identifier(ones),
           "The all-one rendering records no identifier by specification");

    common::uuid_octets mixed{};
    mixed.value[15] = 0x01U;
    expect(!common::uuid_records_no_identifier(mixed),
           "A UUID that distinguishes machines must survive the absence "
           "check");
}

void test_uuid_boundary_validator() {
    namespace common = syscape::detail::hardware_common;

    const auto lowercase =
        common::validate_uuid_text(std::string(
            "03000200-0400-0500-0006-000700080009"));
    expect(lowercase.has_value() &&
               *lowercase == "03000200-0400-0500-0006-000700080009",
           "A canonical rendering passes through unchanged");

    const auto uppercase =
        common::validate_uuid_text(std::string(
            "0300020A-040B-050C-000D-00070008000E"));
    expect(uppercase.has_value() &&
               *uppercase == "0300020a-040b-050c-000d-00070008000e",
           "Uppercase renderings normalize into comparable lowercase");

    const auto kernel_style =
        common::validate_uuid_text(std::string(
            "5b0dcc2a-e8e1-41ae-805f-743d3d3d22f5"));
    expect(kernel_style.has_value(),
           "A typical firmware-recorded UUID validates");

    expect(!common::validate_uuid_text(std::string(
                "03000200-0400-0500-0006-00070008000")),
           "A rendering shorter than thirty-six characters is malformed");

    expect(!common::validate_uuid_text(std::string(
                "03000200-0400-0500-0006-0007000800099")),
           "A rendering longer than thirty-six characters is malformed");

    expect(!common::validate_uuid_text(std::string(
                "03000200/0400/0500/0006/000700080009")),
           "Separators other than hyphens contradict the recorded format");

    expect(!common::validate_uuid_text(std::string(
                "030002g0-0400-0500-0006-000700080009")),
           "Characters outside hexadecimal are malformed platform data");

    expect(!common::validate_uuid_text(std::string(
                "03000200X0400X0500X0006X000700080009")),
           "Hyphens at foreign positions are malformed platform data");

    const auto zero_marker =
        common::validate_uuid_text(std::string(
            "00000000-0000-0000-0000-000000000000"));
    expect(zero_marker.error() == syscape::errc::not_found,
           "The all-zero marker records no identifier and reports "
           "not_found");

    const auto ff_marker =
        common::validate_uuid_text(std::string(
            "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF"));
    expect(ff_marker.error() == syscape::errc::not_found,
           "The all-one marker records no identifier and reports "
           "not_found");
}

/// Reads one DMI-id attribute through plain POSIX calls, independently of
/// the backend's reader.
bool independent_attribute(const char* attribute, std::string& output,
                           int& saved_errno) {
    const std::string path =
        std::string("/sys/class/dmi/id/") + attribute;
    const int descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor < 0) {
        saved_errno = errno;
        return false;
    }
    output.clear();
    char buffer[4096];
    for (;;) {
        const ssize_t count = ::read(descriptor, buffer, sizeof(buffer));
        if (count < 0) {
            saved_errno = errno;
            ::close(descriptor);
            return false;
        }
        if (count == 0) { break; }
        output.append(buffer, static_cast<std::size_t>(count));
    }
    ::close(descriptor);
    saved_errno = 0;
    return true;
}

std::string trim(std::string value) {
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t' ||
            value.back() == '\r' || value.back() == '\n')) {
        value.pop_back();
    }
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '\t' ||
            value.front() == '\r' || value.front() == '\n')) {
        value.erase(value.begin());
    }
    return value;
}

/// Cross-checks one identity-text query against an independent read of its
/// documented attribute file.
void check_text_query(const char* label, const char* attribute,
                      const syscape::result<std::string>& answer) {
    std::string recorded;
    int saved_errno = 0;
    if (independent_attribute(attribute, recorded, saved_errno)) {
        const std::string trimmed = trim(recorded);
        if (trimmed.empty()) {
            expect(!answer &&
                       answer.error() == syscape::errc::not_found,
                   label);
            return;
        }
        expect(answer.has_value(), label);
        if (answer) { expect(*answer == trimmed, label); }
        return;
    }
    if (saved_errno == ENOENT || saved_errno == ENOTDIR) {
        expect(!answer && answer.error() == syscape::errc::not_found,
               label);
        return;
    }
    // Restricted attributes preserve their native permission failure so
    // unprivileged callers can distinguish absence from denial.
    expect(!answer &&
               answer.error() ==
                   std::error_code(saved_errno, std::generic_category()),
           label);
}

void run_live_checks() {
    namespace backend = syscape::detail::hardware_backend;

    const syscape::result<bool> interface =
        backend::dmi_interface_present();
    if (!interface) {
        expect(false, "The DMI-id interface probe must preserve native errors");
        return;
    }
    if (!*interface) {
        // Machines without firmware DMI records report the capability as
        // unsupported everywhere instead of pretending facts exist.
        expect(!backend::system_manufacturer() &&
                   backend::system_manufacturer().error() ==
                       syscape::errc::not_supported,
               "A machine without DMI records must report not_supported");
        return;
    }

    check_text_query(
        "system_manufacturer must match an independent read of sys_vendor",
        "sys_vendor", backend::system_manufacturer());
    check_text_query(
        "system_product_name must match an independent read of product_name",
        "product_name", backend::system_product_name());
    check_text_query(
        "system_product_version must match an independent read of "
        "product_version",
        "product_version", backend::system_product_version());
    check_text_query(
        "motherboard_manufacturer must match an independent read of "
        "board_vendor",
        "board_vendor", backend::motherboard_manufacturer());
    check_text_query(
        "motherboard_product_name must match an independent read of "
        "board_name",
        "board_name", backend::motherboard_product_name());
    check_text_query(
        "motherboard_version must match an independent read of "
        "board_version",
        "board_version", backend::motherboard_version());
    check_text_query(
        "firmware_vendor must match an independent read of bios_vendor",
        "bios_vendor", backend::firmware_vendor());
    check_text_query(
        "firmware_version must match an independent read of bios_version",
        "bios_version", backend::firmware_version());
    check_text_query(
        "firmware_release_date must match an independent read of bios_date",
        "bios_date", backend::firmware_release_date());

    const syscape::result<
        syscape::detail::hardware_common::chassis_classification>
        chassis = backend::chassis_form_factor();
    std::string recorded_type;
    int type_errno = 0;
    if (independent_attribute("chassis_type", recorded_type, type_errno) &&
        type_errno == 0 && !trim(recorded_type).empty()) {
        expect(chassis.has_value(),
               "A recorded chassis type must classify successfully");
        if (chassis) {
            const unsigned long parsed =
                std::stoul(trim(recorded_type));
            if (parsed >= 1UL && parsed <= 36UL) {
                const syscape::result<
                    syscape::detail::hardware_common::
                        chassis_classification>
                    expected = syscape::detail::hardware_common::
                        classify_chassis(static_cast<std::uint8_t>(parsed));
                expect(expected.has_value() && *expected == *chassis,
                       "The reported form factor must match the recorded "
                       "classification byte");
            } else {
                expect(!chassis &&
                           chassis.error() == syscape::errc::malformed_data,
                       "A classification outside the documented table is "
                       "malformed platform data");
            }
        }
    }

    const syscape::result<std::string> uuid = backend::hardware_uuid();
    std::string recorded_uuid;
    int uuid_errno = 0;
    if (independent_attribute("product_uuid", recorded_uuid,
                              uuid_errno) &&
        uuid_errno == 0 && !trim(recorded_uuid).empty()) {
        expect(uuid.has_value(),
               "A readable product_uuid attribute must yield a UUID");
        if (uuid) {
            const std::string recorded = trim(recorded_uuid);
            expect(uuid->size() == recorded.size(),
                   "The rendered UUID must keep the recorded length");
            bool same_letters = true;
            for (std::size_t index = 0; index < uuid->size() &&
                                        index < recorded.size();
                 ++index) {
                const char left = (*uuid)[index];
                const char right = recorded[index];
                const char lower_left =
                    left >= 'A' && left <= 'F'
                        ? static_cast<char>(left - 'A' + 'a')
                        : left;
                const char lower_right =
                    right >= 'A' && right <= 'F'
                        ? static_cast<char>(right - 'A' + 'a')
                        : right;
                if (lower_left != lower_right) { same_letters = false; }
            }
            expect(same_letters,
                   "The rendered UUID must carry the recorded hexadecimal "
                   "digits regardless of letter case");
            expect(*uuid != "00000000-0000-0000-0000-000000000000" &&
                       *uuid != "ffffffff-ffff-ffff-ffff-ffffffffffff",
                   "A returned UUID must distinguish something, never "
                   "carry an absence marker");
        }
    } else if (uuid_errno == EACCES) {
        expect(!uuid &&
                   uuid.error() ==
                       std::error_code(EACCES, std::generic_category()),
               "The privileged-only product_uuid attribute must preserve "
               "its native permission failure");
    }

    // Identity facts describe fixed firmware records, so repeated calls
    // agree between reboots.
    const syscape::result<std::string> first_vendor =
        backend::system_manufacturer();
    const syscape::result<std::string> second_vendor =
        backend::system_manufacturer();
    expect(first_vendor.has_value() == second_vendor.has_value() &&
               (!first_vendor || *first_vendor == *second_vendor),
           "Repeated system-manufacturer queries must agree");
    const auto first_chassis = backend::chassis_form_factor();
    const auto second_chassis = backend::chassis_form_factor();
    expect(first_chassis.has_value() == second_chassis.has_value() &&
               (!first_chassis || *first_chassis == *second_chassis),
           "Repeated chassis queries must agree");
}

void test_pci_bdf_parser() {
    namespace backend = syscape::detail::hardware_backend;

    std::uint16_t domain = 0;
    std::uint8_t bus = 0;
    std::uint8_t device = 0;
    std::uint8_t function = 0;

    expect(backend::parse_pci_bdf("0000:00:00.0", domain, bus, device, function),
           "Standard PCI BDF must parse");
    expect(domain == 0 && bus == 0 && device == 0 && function == 0,
           "Parsed values for 0000:00:00.0 must match");

    expect(backend::parse_pci_bdf("0001:0a:1f.7", domain, bus, device, function),
           "Hex values in PCI BDF must parse");
    expect(domain == 1 && bus == 0x0A && device == 0x1F && function == 7,
           "Parsed values for 0001:0a:1f.7 must match");

    expect(!backend::parse_pci_bdf("0000:00:00", domain, bus, device, function),
           "Missing function must fail");
    expect(!backend::parse_pci_bdf("0000:00.0", domain, bus, device, function),
           "Missing device colon must fail");
    expect(!backend::parse_pci_bdf("gggg:00:00.0", domain, bus, device, function),
           "Non-hex domain must fail");
    expect(!backend::parse_pci_bdf("0000:00:00.g", domain, bus, device, function),
           "Non-hex function must fail");
    expect(!backend::parse_pci_bdf("0000:00:20.0", domain, bus, device, function),
           "PCI device numbers above 31 must fail");
    expect(!backend::parse_pci_bdf("0000:00:00.8", domain, bus, device, function),
           "PCI function numbers above 7 must fail");
}

void test_usb_attribute_parsers() {
    namespace backend = syscape::detail::hardware_backend;

    const auto low_speed = backend::parse_usb_speed_mbps("1.5\n");
    expect(low_speed.has_value() && low_speed->has_value() &&
               **low_speed == 1.5,
           "USB low-speed signaling rate must preserve 1.5 Mbps");
    const auto high_speed = backend::parse_usb_speed_mbps("10000");
    expect(high_speed.has_value() && high_speed->has_value() &&
               **high_speed == 10000.0,
           "Integral USB signaling rates must parse");
    const auto unknown_speed = backend::parse_usb_speed_mbps("unknown\n");
    expect(unknown_speed.has_value() && !unknown_speed->has_value(),
           "An unnegotiated USB speed must remain unknown");
    expect(!backend::parse_usb_speed_mbps("1.5junk").has_value(),
           "USB speed trailing garbage must fail");

    const auto nested_port = backend::parse_usb_port("1.2.7\n");
    expect(nested_port.has_value() && nested_port->has_value() &&
               **nested_port == 7U,
           "Nested USB devpath must expose its immediate upstream port");
    const auto root_port = backend::parse_usb_port("0\n");
    expect(root_port.has_value() && !root_port->has_value(),
           "A root hub devpath must not fabricate an upstream port");
    expect(!backend::parse_usb_port("1.bad").has_value(),
           "Malformed USB devpath must fail");
}

void test_pci_class_classifier() {
    namespace common = syscape::detail::hardware_common;
    using pci_class = syscape::hardware::pci_class;

    expect(common::classify_pci_class(0x01U) == pci_class::mass_storage,
           "0x01 must map to mass_storage");
    expect(common::classify_pci_class(0x02U) == pci_class::network_controller,
           "0x02 must map to network_controller");
    expect(common::classify_pci_class(0x03U) == pci_class::display_controller,
           "0x03 must map to display_controller");
    expect(common::classify_pci_class(0x04U) == pci_class::multimedia_controller,
           "0x04 must map to multimedia_controller");
    expect(common::classify_pci_class(0x06U) == pci_class::bridge,
           "0x06 must map to bridge");
    expect(common::classify_pci_class(0x0CU) == pci_class::serial_bus_controller,
           "0x0C must map to serial_bus_controller");
    expect(common::classify_pci_class(0x13U) == pci_class::non_essential_instrumentation,
           "0x13 must map to non_essential_instrumentation");
    expect(common::classify_pci_class(0xFFU) == pci_class::unknown,
           "Unrecognized class code must map to unknown");
}

void test_memory_classifiers() {
    namespace common = syscape::detail::hardware_common;
    using form_factor = syscape::hardware::memory_form_factor;
    using mem_type = syscape::hardware::memory_type;

    expect(common::classify_memory_form_factor(0x09U) == form_factor::proprietary,
           "0x09 must map to proprietary");
    expect(common::classify_memory_form_factor(0x0AU) == form_factor::dimm,
           "0x0A must map to dimm");
    expect(common::classify_memory_form_factor(0x0EU) == form_factor::sodimm,
           "0x0E must map to sodimm");
    expect(common::classify_memory_form_factor(0x12U) == form_factor::camm,
           "0x12 must map to camm");
    expect(common::classify_memory_form_factor(0xFFU) == form_factor::unknown,
           "0xFF form factor must map to unknown");

    expect(common::classify_memory_type(0x12U) == mem_type::ddr,
           "0x12 must map to ddr");
    expect(common::classify_memory_type(0x13U) == mem_type::ddr2,
           "0x13 must map to ddr2");
    expect(common::classify_memory_type(0x18U) == mem_type::ddr3,
           "0x18 must map to ddr3");
    expect(common::classify_memory_type(0x1AU) == mem_type::ddr4,
           "0x1A must map to ddr4");
    expect(common::classify_memory_type(0x22U) == mem_type::ddr5,
           "0x22 must map to ddr5");
    expect(common::classify_memory_type(0x23U) == mem_type::lpddr5,
           "0x23 must map to lpddr5");
    expect(common::classify_memory_type(0xFFU) == mem_type::unknown,
           "0xFF memory type must map to unknown");
}

void test_smbios_type17_synthetic_parser() {
    namespace common = syscape::detail::hardware_common;

    const std::uint8_t little16[2] = {0x00U, 0x40U};
    const std::uint8_t little32[4] = {0x00U, 0x80U, 0x00U, 0x00U};
    expect(common::read_le_u16(little16) == 0x4000U,
           "SMBIOS 16-bit fields must decode independently of host byte order");
    expect(common::read_le_u32(little32) == 0x00008000U,
           "SMBIOS 32-bit fields must decode independently of host byte order");

    struct synthetic_structure {
        std::vector<std::uint8_t> formatted;
        std::vector<std::string> strings;
    };

    const auto build_table = [](const std::vector<synthetic_structure>& structures) {
        std::vector<std::uint8_t> blob;
        for (const auto& s : structures) {
            std::vector<std::uint8_t> fmt = s.formatted;
            fmt[1] = static_cast<std::uint8_t>(fmt.size());
            blob.insert(blob.end(), fmt.begin(), fmt.end());
            for (const auto& str : s.strings) {
                blob.insert(blob.end(), str.begin(), str.end());
                blob.push_back(0U);
            }
            if (s.strings.empty()) { blob.push_back(0U); }
            blob.push_back(0U);
        }
        blob.push_back(127U);
        blob.push_back(4U);
        blob.push_back(0xFEU);
        blob.push_back(0xFFU);
        blob.push_back(0U);
        blob.push_back(0U);
        return blob;
    };

    // Slot 1: 16 GB DDR4-3200 DIMM
    std::vector<std::uint8_t> dimm1_fmt(36, 0);
    dimm1_fmt[0] = 17U; // Type 17
    dimm1_fmt[1] = 36U; // Length
    dimm1_fmt[0x0C] = 0x00U;
    dimm1_fmt[0x0D] = 0x40U; // 0x4000 = 16384 MB (16 GB)
    dimm1_fmt[0x0E] = 0x0AU; // DIMM
    dimm1_fmt[0x10] = 1U;    // Locator ("DIMM 0")
    dimm1_fmt[0x11] = 2U;    // Bank ("BANK 0")
    dimm1_fmt[0x12] = 0x1AU; // DDR4
    dimm1_fmt[0x15] = 0x80U;
    dimm1_fmt[0x16] = 0x0CU; // 3200 MT/s
    dimm1_fmt[0x17] = 3U;    // Manufacturer ("Crucial")
    dimm1_fmt[0x18] = 4U;    // Serial Number ("12345678")
    dimm1_fmt[0x1A] = 5U;    // Part Number ("CT16G4")
    dimm1_fmt[0x20] = 0x80U;
    dimm1_fmt[0x21] = 0x0CU; // Configured speed 3200 MT/s

    // Slot 2: Empty slot
    std::vector<std::uint8_t> dimm2_fmt(36, 0);
    dimm2_fmt[0] = 17U;
    dimm2_fmt[1] = 36U;
    dimm2_fmt[0x0C] = 0x00U;
    dimm2_fmt[0x0D] = 0x00U; // Size 0 -> empty socket
    dimm2_fmt[0x0E] = 0x0AU; // DIMM
    dimm2_fmt[0x10] = 1U;    // Locator ("DIMM 1")
    dimm2_fmt[0x11] = 2U;    // Bank ("BANK 1")
    dimm2_fmt[0x12] = 0x02U; // Unknown type

    // Slot 3: 64 GB DDR5 Extended Size
    std::vector<std::uint8_t> dimm3_fmt(36, 0);
    dimm3_fmt[0] = 17U;
    dimm3_fmt[1] = 36U;
    dimm3_fmt[0x0C] = 0xFFU;
    dimm3_fmt[0x0D] = 0x7FU; // 0x7FFF -> Extended Size used
    dimm3_fmt[0x0E] = 0x0AU; // DIMM
    dimm3_fmt[0x10] = 1U;    // Locator ("DIMM 2")
    dimm3_fmt[0x11] = 2U;    // Bank ("BANK 2")
    dimm3_fmt[0x12] = 0x22U; // DDR5
    dimm3_fmt[0x15] = 0xC0U;
    dimm3_fmt[0x16] = 0x12U; // 4800 MT/s (0x12C0)
    // Extended size at 0x1C (4 bytes): 65536 MB (64 GB) -> 0x00010000
    dimm3_fmt[0x1C] = 0x00U;
    dimm3_fmt[0x1D] = 0x00U;
    dimm3_fmt[0x1E] = 0x01U;
    dimm3_fmt[0x1F] = 0x00U;

    // Slot 4: Installed module whose capacity is unknown.
    std::vector<std::uint8_t> dimm4_fmt(36, 0);
    dimm4_fmt[0] = 17U;
    dimm4_fmt[0x0C] = 0xFFU;
    dimm4_fmt[0x0D] = 0xFFU;
    dimm4_fmt[0x10] = 1U;

    const std::vector<std::uint8_t> table = build_table({
        {dimm1_fmt, {"DIMM 0", "BANK 0", "Crucial", "12345678", "CT16G4"}},
        {dimm2_fmt, {"DIMM 1", "BANK 1"}},
        {dimm3_fmt, {"DIMM 2", "BANK 2"}},
        {dimm4_fmt, {"DIMM 3"}}
    });

    const auto parsed = common::parse_smbios_memory_devices(table.data(), table.size());
    expect(parsed.has_value(), "Synthetic SMBIOS Type 17 table must parse successfully");
    if (parsed) {
        expect(parsed->size() == 4U, "Table must contain 4 memory devices");
        // DIMM 0
        expect((*parsed)[0].locator == "DIMM 0", "DIMM 0 locator must match");
        expect((*parsed)[0].bank_locator == "BANK 0", "DIMM 0 bank must match");
        expect((*parsed)[0].size_bytes.has_value() && *(*parsed)[0].size_bytes == 16384ULL * 1024ULL * 1024ULL,
               "DIMM 0 size must be 16 GB");
        expect((*parsed)[0].state == syscape::hardware::memory_device_state::installed,
               "DIMM 0 must be marked installed");
        expect((*parsed)[0].form_factor == syscape::hardware::memory_form_factor::dimm, "DIMM 0 form factor must match");
        expect((*parsed)[0].type == syscape::hardware::memory_type::ddr4, "DIMM 0 type must be DDR4");
        expect((*parsed)[0].speed.has_value() && (*parsed)[0].speed->value == 3200U &&
                   (*parsed)[0].speed->unit == syscape::hardware::memory_speed_unit::unknown,
               "DIMM 0 speed must preserve 3200 with an unknown unit");
        expect((*parsed)[0].configured_speed.has_value() &&
                   (*parsed)[0].configured_speed->value == 3200U,
               "DIMM 0 configured speed must preserve 3200");
        expect((*parsed)[0].manufacturer == "Crucial", "DIMM 0 manufacturer must match");
        expect((*parsed)[0].serial_number == "12345678", "DIMM 0 serial number must match");
        expect((*parsed)[0].part_number == "CT16G4", "DIMM 0 part number must match");

        // DIMM 1 (empty slot)
        expect((*parsed)[1].locator == "DIMM 1", "DIMM 1 locator must match");
        expect(!(*parsed)[1].size_bytes.has_value(), "Empty slot must have nullopt size");
        expect((*parsed)[1].state == syscape::hardware::memory_device_state::not_installed,
               "Empty slot must be distinguished from unknown capacity");
        expect((*parsed)[1].form_factor == syscape::hardware::memory_form_factor::dimm, "DIMM 1 form factor must match");

        // DIMM 2 (extended size 64 GB)
        expect((*parsed)[2].locator == "DIMM 2", "DIMM 2 locator must match");
        expect((*parsed)[2].size_bytes.has_value() && *(*parsed)[2].size_bytes == 65536ULL * 1024ULL * 1024ULL,
               "DIMM 2 size must be 64 GB");
        expect((*parsed)[2].type == syscape::hardware::memory_type::ddr5, "DIMM 2 type must be DDR5");
        expect((*parsed)[2].speed.has_value() && (*parsed)[2].speed->value == 4800U,
               "DIMM 2 speed must preserve 4800");

        expect((*parsed)[3].state == syscape::hardware::memory_device_state::installed &&
                   !(*parsed)[3].size_bytes.has_value(),
               "Unknown installed capacity must not be confused with an empty slot");
    }

    std::vector<std::uint8_t> missing_end = table;
    missing_end.resize(missing_end.size() - 6U);
    expect(!common::parse_smbios_memory_devices(
                missing_end.data(), missing_end.size()).has_value(),
           "A memory table without the SMBIOS end marker must fail");

    std::vector<std::uint8_t> trailing_garbage = table;
    trailing_garbage.push_back(0xA5U);
    expect(!common::parse_smbios_memory_devices(
                trailing_garbage.data(), trailing_garbage.size()).has_value(),
           "Nonzero data after the SMBIOS end marker must fail");

    std::vector<std::uint8_t> short_extended_fmt(0x15U, 0U);
    short_extended_fmt[0] = 17U;
    short_extended_fmt[0x0C] = 0xFFU;
    short_extended_fmt[0x0D] = 0x7FU;
    short_extended_fmt[0x10] = 1U;
    const auto short_extended = build_table({
        {short_extended_fmt, {"DIMM short"}}
    });
    expect(!common::parse_smbios_memory_devices(
                short_extended.data(), short_extended.size()).has_value(),
           "The Extended Size sentinel without its field must fail");

    std::vector<std::uint8_t> reserved_extended_fmt(0x20U, 0U);
    reserved_extended_fmt[0] = 17U;
    reserved_extended_fmt[0x0C] = 0xFFU;
    reserved_extended_fmt[0x0D] = 0x7FU;
    reserved_extended_fmt[0x10] = 1U;
    reserved_extended_fmt[0x1FU] = 0x80U;
    const auto reserved_extended = build_table({
        {reserved_extended_fmt, {"DIMM reserved"}}
    });
    expect(!common::parse_smbios_memory_devices(
                reserved_extended.data(), reserved_extended.size()).has_value(),
           "A set reserved bit in Extended Size must fail");

    std::vector<std::uint8_t> empty_strings_fmt(0x15U, 0U);
    empty_strings_fmt[0] = 17U;
    empty_strings_fmt[0x10U] = 1U;
    const auto empty_strings = build_table({
        {empty_strings_fmt, {}}
    });
    expect(common::parse_smbios_memory_devices(
               empty_strings.data(), empty_strings.size()).error() ==
               syscape::errc::malformed_data,
           "A nonzero Type 17 index into an empty string set must fail");
}

void test_live_device_inventory() {
    // PCI devices
    const auto pci_res = syscape::hardware::pci_devices();
    expect(pci_res.has_value() || pci_res.error() == syscape::errc::not_supported,
           "pci_devices() must succeed or report not_supported");
    if (pci_res) {
        expect(!pci_res->empty(), "A Linux system should expose observable PCI devices");
        for (std::size_t i = 1; i < pci_res->size(); ++i) {
            expect(!syscape::detail::hardware_common::compare_pci_devices((*pci_res)[i], (*pci_res)[i - 1]),
                   "PCI devices must be sorted deterministically by BDF");
        }
        for (const auto& dev : *pci_res) {
            expect(dev.vendor_id != 0U, "PCI device vendor ID must be nonzero");
            expect(dev.device_id != 0U, "PCI device ID must be nonzero");
        }
    }

    // USB devices
    const auto usb_res = syscape::hardware::usb_devices();
    expect(usb_res.has_value() || usb_res.error() == syscape::errc::not_supported,
           "usb_devices() must succeed or report not_supported");
    if (usb_res) {
        for (std::size_t i = 1; i < usb_res->size(); ++i) {
            expect(!syscape::detail::hardware_common::compare_usb_devices((*usb_res)[i], (*usb_res)[i - 1]),
                   "USB devices must be sorted deterministically");
        }
    }

    // Memory devices
    const auto mem_res = syscape::hardware::memory_devices();
    expect(mem_res.has_value() ||
           mem_res.error() == syscape::errc::not_supported ||
           mem_res.error() == std::error_code(EACCES, std::generic_category()) ||
           mem_res.error() == std::error_code(EPERM, std::generic_category()),
           "memory_devices() must succeed, report not_supported, or preserve permission denied");
    if (mem_res) {
        for (std::size_t i = 1; i < mem_res->size(); ++i) {
            expect(!syscape::detail::hardware_common::compare_memory_devices((*mem_res)[i], (*mem_res)[i - 1]),
                   "Memory devices must be sorted deterministically by locator");
        }
    }
}

} // namespace

int main() {
    test_number_parser();
    test_dmi_interface_probe();
    test_chassis_classifier();
    test_uuid_renderer();
    test_uuid_boundary_validator();
    test_pci_bdf_parser();
    test_usb_attribute_parsers();
    test_pci_class_classifier();
    test_memory_classifiers();
    test_smbios_type17_synthetic_parser();
    run_live_checks();
    test_live_device_inventory();
    return failures == 0 ? 0 : 1;
}
