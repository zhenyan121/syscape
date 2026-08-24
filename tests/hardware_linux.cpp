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

    if (!backend::dmi_interface_present()) {
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

} // namespace

int main() {
    test_number_parser();
    test_chassis_classifier();
    test_uuid_renderer();
    test_uuid_boundary_validator();
    run_live_checks();
    return failures == 0 ? 0 : 1;
}
