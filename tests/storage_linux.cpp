#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <sys/stat.h>

#include <syscape/storage.hpp>
#include <syscape/detail/storage/linux.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_number_parser() {
    namespace backend = syscape::detail::storage_backend;

    const auto typical = backend::parse_number<std::uint64_t>("2000409264\n");
    expect(typical && *typical == 2000409264ULL,
           "A documented sector count must parse as an unsigned integer");

    const auto small = backend::parse_number<std::uint32_t>("512");
    expect(small && *small == 512U,
           "A documented block size must fit the 32-bit field");

    const auto zero = backend::parse_number<std::uint64_t>("0\n");
    expect(zero && *zero == 0U,
           "Zero is parsed so callers can apply its no-medium meaning");

    const auto overflow =
        backend::parse_number<std::uint32_t>("99999999999");
    expect(!overflow && overflow.error() == syscape::errc::value_too_large,
           "Block sizes beyond 32 bits must report value_too_large");

    const auto wide_overflow = backend::parse_number<std::uint64_t>(
        "99999999999999999999999");
    expect(!wide_overflow &&
               wide_overflow.error() == syscape::errc::value_too_large,
           "Sector counts beyond 64 bits must report value_too_large");

    const auto text = backend::parse_number<std::uint64_t>("many sectors\n");
    expect(!text && text.error() == syscape::errc::malformed_data,
           "A nonnumeric size rendering must be malformed platform data");

    const auto trailing = backend::parse_number<std::uint64_t>("12 kb");
    expect(!trailing &&
               trailing.error() == syscape::errc::malformed_data,
           "Trailing text after a number must be malformed platform data");

    const auto signed_value = backend::parse_number<std::uint64_t>("-5\n");
    expect(!signed_value &&
               signed_value.error() == syscape::errc::malformed_data,
           "A signed rendering cannot describe an unsigned attribute");

    const auto empty = backend::parse_number<std::uint64_t>("   \n");
    expect(!empty && empty.error() == syscape::errc::malformed_data,
           "An empty numeric attribute must be malformed");
}

void test_flag_parser() {
    namespace backend = syscape::detail::storage_backend;

    const auto set = backend::parse_flag("1\n");
    expect(set && *set, "The documented one rendering must mean true");

    const auto clear = backend::parse_flag("0\n");
    expect(clear && !*clear, "The documented zero rendering must mean false");

    const auto word = backend::parse_flag("true\n");
    expect(!word && word.error() == syscape::errc::malformed_data,
           "Only the documented digits are accepted flags");

    const auto other_digit = backend::parse_flag("2");
    expect(!other_digit &&
               other_digit.error() == syscape::errc::malformed_data,
           "Any digit other than zero and one must be malformed");
}

void test_subsystem_classifier() {
    namespace backend = syscape::detail::storage_backend;
    using classification = syscape::detail::storage_common::bus_classification;

    const auto nvme = backend::classify_subsystem("nvme");
    expect(nvme == classification::nvme,
           "The kernel-recorded NVMe subsystem must map to nvme");

    const auto scsi = backend::classify_subsystem("scsi");
    expect(scsi == classification::scsi,
           "The kernel-recorded SCSI subsystem must map to scsi");

    const auto mmc = backend::classify_subsystem("mmc");
    expect(mmc == classification::mmc,
           "The kernel-recorded MMC subsystem must map to mmc");

    expect(backend::classify_subsystem("virtio") ==
                   classification::unknown,
           "Subsystems outside the portable vocabulary must stay unknown");

    expect(backend::classify_subsystem("block") == classification::unknown,
           "The block class itself carries no transport information");

    expect(backend::backing_subsystem_unchanged("nvme", true, "nvme"),
           "An unchanged resolved subsystem must survive final validation");
    expect(!backend::backing_subsystem_unchanged("nvme", false, ""),
           "A backing subsystem removed during collection must be skipped");
    expect(!backend::backing_subsystem_unchanged("scsi", true, "nvme"),
           "A replaced backing subsystem must be treated as a race");
}

void test_capacity_conversion() {
    namespace backend = syscape::detail::storage_backend;

    const auto bytes = backend::capacity_in_bytes(2000409264ULL);
    expect(bytes && *bytes == 1024209543168ULL,
           "Sector counts must convert through exactly 512-byte units");

    const auto zero = backend::capacity_in_bytes(0ULL);
    expect(zero && *zero == 0ULL,
           "A zero sector count converts to a valid zero-byte capacity");

    const auto huge = backend::capacity_in_bytes(~0ULL);
    expect(!huge && huge.error() == syscape::errc::value_too_large,
           "Conversions that would truncate must report value_too_large");
}

void test_text_application() {
    namespace backend = syscape::detail::storage_backend;

    bool present = false;
    std::string destination;

    backend::apply_text(backend::attribute_read{true, "YMTC model   \n"},
                        present, destination);
    expect(present && destination == "YMTC model",
           "Firmware string padding must be trimmed away");

    present = false;
    destination.clear();
    backend::apply_text(backend::attribute_read{true, "     \n"}, present,
                        destination);
    expect(!present && destination.empty(),
           "A wholly blank firmware string must record an absent field");

    present = false;
    destination.clear();
    backend::apply_text(backend::attribute_read{}, present, destination);
    expect(!present,
           "An absent attribute must leave the field unrecorded");
}

void test_boundary_validation() {
    namespace common = syscape::detail::storage_common;

    using records = std::vector<common::drive_record>;

    const syscape::result<records> accepted =
        common::validate_drive_records(records{});
    expect(accepted.has_value(),
           "An empty enumeration is valid data and must be accepted");

    syscape::result<records> bad_identifier(records{});
    bad_identifier->push_back(common::drive_record{});
    bad_identifier->back().identifier = "\xff\xfe";

    const auto encoding_checked =
        common::validate_drive_records(std::move(bad_identifier));
    expect(!encoding_checked &&
               encoding_checked.error() == syscape::errc::invalid_encoding,
           "Drive identifiers must be well-formed UTF-8");

    syscape::result<records> empty_identifier(records{});
    empty_identifier->push_back(common::drive_record{});

    const auto emptiness_checked =
        common::validate_drive_records(std::move(empty_identifier));
    expect(!emptiness_checked &&
               emptiness_checked.error() == syscape::errc::invalid_encoding,
           "An empty identifier cannot name a device");

    syscape::result<records> extended_sector(records{});
    extended_sector->push_back(common::drive_record{});
    extended_sector->back().identifier = "sg0";
    extended_sector->back().has_logical_sector_size_bytes = true;
    extended_sector->back().logical_sector_size_bytes = 520U;

    const auto extended_checked =
        common::validate_drive_records(std::move(extended_sector));
    expect(extended_checked.has_value(),
           "A non-power-of-two block size can be valid platform data");

    syscape::result<records> zero_block(records{});
    zero_block->push_back(common::drive_record{});
    zero_block->back().identifier = "sdz";
    zero_block->back().has_physical_sector_size_bytes = true;

    const auto nonzero_checked =
        common::validate_drive_records(std::move(zero_block));
    expect(!nonzero_checked &&
               nonzero_checked.error() == syscape::errc::malformed_data,
           "A zero block size contradicts every platform's addressing "
           "rules and must be malformed data");

    syscape::result<records> valid_zero_capacity(records{});
    valid_zero_capacity->push_back(common::drive_record{});
    valid_zero_capacity->back().identifier = "sr0";
    valid_zero_capacity->back().has_capacity_bytes = true;

    const auto mediumless =
        common::validate_drive_records(std::move(valid_zero_capacity));
    expect(mediumless.has_value(),
           "A zero capacity is valid data for a device without media");
}

/// Reads one sysfs attribute independently of the backend's reader.
bool independent_attribute(const std::string& entry, const char* attribute,
                           std::string& output) {
    const std::string path = "/sys/block/" + entry + "/" + attribute;
    std::ifstream stream(path);
    if (!stream.is_open()) { return false; }
    std::getline(stream, output);
    return true;
}

/// Resolves whether the kernel recorded a backing device node for the
/// entry, independently of the backend's lstat-based rule.
bool independent_backing_node(const std::string& entry) {
    struct stat backing;
    return ::lstat(("/sys/block/" + entry + "/device").c_str(), &backing) == 0;
}

/// Resolves the final component of the backing device's subsystem link,
/// independently of the backend's reader.
bool independent_subsystem(const std::string& entry, std::string& output) {
    const std::string path =
        "/sys/block/" + entry + "/device/subsystem";
    char buffer[4096];
    const ssize_t length =
        ::readlink(path.c_str(), buffer, sizeof(buffer));
    if (length <= 0 ||
        static_cast<std::size_t>(length) >= sizeof(buffer)) {
        return false;
    }
    const std::string target(buffer, static_cast<std::size_t>(length));
    const std::size_t base = target.rfind('/');
    output = base == std::string::npos ? target : target.substr(base + 1U);
    return true;
}

std::string trim(std::string value) {
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t' ||
            value.back() == '\r' || value.back() == '\n')) {
        value.pop_back();
    }
    return value;
}

void test_live_queries() {
    const syscape::result<std::vector<syscape::storage::drive_entry>>
        listed = syscape::storage::drives();
    expect(listed || listed.error() == syscape::errc::not_supported,
           "Drive enumeration must succeed or report honestly that the "
           "platform exposes no acceptable source");
    if (!listed) { return; }

    std::string previous_identifier;
    for (const syscape::storage::drive_entry& drive : *listed) {
        expect(drive.identifier != "." && drive.identifier != ".." &&
                   drive.identifier.find('/') == std::string::npos,
               "Drive identifiers must be plain directory names");
        expect(previous_identifier <= drive.identifier,
               "Drive entries must be ordered by ascending identifier");
        expect(previous_identifier != drive.identifier,
               "Drive identifiers must be unique within one snapshot");
        previous_identifier = drive.identifier;

        // Every enumerated entry must satisfy the hardware-backing contract
        // as observed independently of the backend's own rule.
        expect(independent_backing_node(drive.identifier),
               "Enumerated drives must have a backing device node");

        std::string recorded;
        if (independent_attribute(drive.identifier, "size", recorded)) {
            const std::uint64_t sectors = std::stoull(recorded);
            expect(drive.has_capacity_bytes,
                   "A drive whose kernel exposes a size attribute must "
                   "record a capacity");
            if (drive.has_capacity_bytes &&
                sectors <= ~0ULL / 512ULL) {
                expect(drive.capacity_bytes == sectors * 512ULL,
                       "A reported capacity must equal the kernel-recorded "
                       "sector count times exactly 512 bytes");
            }
        } else {
            expect(!drive.has_capacity_bytes,
                   "A drive without a size attribute must record no "
                   "capacity");
        }

        if (independent_attribute(drive.identifier,
                                  "queue/logical_block_size", recorded)) {
            const std::uint32_t block_size =
                static_cast<std::uint32_t>(std::stoul(recorded));
            expect(drive.has_logical_sector_size_bytes &&
                       drive.logical_sector_size_bytes == block_size,
                   "A reported logical block size must match an "
                   "independent read of the same attribute");
        }

        if (independent_attribute(drive.identifier,
                                  "queue/physical_block_size", recorded)) {
            const std::uint32_t block_size =
                static_cast<std::uint32_t>(std::stoul(recorded));
            expect(drive.has_physical_sector_size_bytes &&
                       drive.physical_sector_size_bytes == block_size,
                   "A reported physical block size must match an "
                   "independent read of the same attribute");
        }

        if (drive.has_logical_sector_size_bytes &&
            drive.has_physical_sector_size_bytes) {
            expect(drive.physical_sector_size_bytes >=
                       drive.logical_sector_size_bytes,
                   "The kernel guarantees the physical block size never "
                   "falls below the logical block size");
        }

        if (drive.has_capacity_bytes && drive.has_logical_sector_size_bytes) {
            expect(drive.capacity_bytes %
                               drive.logical_sector_size_bytes ==
                           0U,
                   "A whole disk's capacity must divide evenly into its "
                   "logical blocks");
        }

        if (independent_attribute(drive.identifier, "queue/rotational",
                                  recorded)) {
            expect(drive.has_rotational &&
                       drive.rotational == (trim(recorded) == "1"),
                   "A reported rotation fact must match an independent "
                   "read of the same attribute");
        }

        if (independent_attribute(drive.identifier, "removable",
                                  recorded)) {
            expect(drive.removable == (trim(recorded) == "1"),
                   "Removability must match an independent read of the "
                   "same attribute");
        }

        std::string subsystem;
        if (independent_subsystem(drive.identifier, subsystem)) {
            const bool recognized = subsystem == "scsi" ||
                                    subsystem == "nvme" ||
                                    subsystem == "mmc";
            if (recognized) {
                expect(static_cast<int>(drive.bus) !=
                           static_cast<int>(
                               syscape::storage::bus_type::unknown),
                       "A drive whose kernel-recorded subsystem belongs to "
                       "the mapped vocabulary must not record unknown");
            } else {
                expect(drive.bus ==
                           syscape::storage::bus_type::unknown,
                       "Subsystems outside the vocabulary must record "
                       "unknown instead of a guessed transport");
            }
        }
    }
}

} // namespace

int main() {
    test_number_parser();
    test_flag_parser();
    test_subsystem_classifier();
    test_capacity_conversion();
    test_text_application();
    test_boundary_validation();
    test_live_queries();
    return failures == 0 ? 0 : 1;
}
