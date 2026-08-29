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

    const auto signed_int = backend::parse_number<std::int64_t>("-1000\n");
    expect(signed_int && *signed_int == -1000,
           "A signed rendering must parse for signed types");

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

void test_partition_scheme_classifier() {
    namespace backend = syscape::detail::storage_backend;
    using scheme = syscape::storage::partition_scheme;

    expect(backend::classify_partition_scheme(
               "6dc136bc-42c9-40ab-b850-6e74eda97dd6") == scheme::gpt,
           "A 36-character standard UUID format must classify as GPT");

    expect(backend::classify_partition_scheme(
               "12345678-01") == scheme::mbr,
           "An 8-hex-digit with partition number suffix must classify as MBR");

    expect(backend::classify_partition_scheme(
               "") == scheme::unknown,
           "An absent UUID must classify as unknown");

    expect(backend::classify_partition_scheme(
               "custom-id-1234") == scheme::unknown,
           "An unrecognized UUID format must classify as unknown");
}

void test_mount_field_decoder() {
    namespace backend = syscape::detail::storage_backend;

    const auto regular = backend::decode_mount_field("/home/user");
    expect(regular && *regular == "/home/user",
           "Plain mount fields without escapes must decode unchanged");

    const auto spaced = backend::decode_mount_field("/mnt/my\\040disk");
    expect(spaced && *spaced == "/mnt/my disk",
           "Documented octal space escape \\040 must decode to space");

    const auto tabbed = backend::decode_mount_field("/mnt/my\\011disk");
    expect(tabbed && *tabbed == "/mnt/my\tdisk",
           "Documented octal tab escape \\011 must decode to tab");

    const auto newlined = backend::decode_mount_field("/mnt/my\\012disk");
    expect(newlined && *newlined == "/mnt/my\ndisk",
           "Documented octal newline escape \\012 must decode to newline");

    const auto backslash = backend::decode_mount_field("/mnt/my\\134disk");
    expect(backslash && *backslash == "/mnt/my\\disk",
           "Documented octal backslash escape \\134 must decode to backslash");

    const auto invalid = backend::decode_mount_field("/mnt/my\\000disk");
    expect(!invalid && invalid.error() == syscape::errc::malformed_data,
           "Undocumented escape sequences must report malformed_data");

    const auto truncated = backend::decode_mount_field("/mnt/my\\04");
    expect(!truncated && truncated.error() == syscape::errc::malformed_data,
           "Truncated escape sequences must report malformed_data");

    const auto parsed = backend::parse_mount_table(
        "/dev/sda1 /mnt/my\\040disk ext4 rw 0 0\n"
        "tmpfs /run tmpfs rw 0 0\n");
    expect(parsed && parsed->size() == 1U,
           "Only block-device mount records must be collected");
    if (parsed && parsed->size() == 1U) {
        expect((*parsed)[0].device_name == "sda1" &&
                   (*parsed)[0].mount_point == "/mnt/my disk" &&
                   (*parsed)[0].fstype == "ext4",
               "Mount-table fields must be decoded and preserved");
    }

    const auto short_record =
        backend::parse_mount_table("/dev/sda1 /mnt ext4\n");
    expect(!short_record &&
               short_record.error() == syscape::errc::malformed_data,
           "Mount records with fewer than six fields must be malformed");
}

void test_partition_boundary_validation() {
    namespace common = syscape::detail::storage_common;
    using records = std::vector<common::partition_record>;

    const syscape::result<records> accepted =
        common::validate_partition_records(records{});
    expect(accepted.has_value(),
           "An empty partition list must be accepted as valid data");

    records bad_id;
    bad_id.push_back(common::partition_record{});
    bad_id.back().identifier = "\xff\xfe";
    bad_id.back().disk_identifier = "sda";
    const auto bad_id_res =
        common::validate_partition_records(bad_id);
    expect(!bad_id_res &&
               bad_id_res.error() == syscape::errc::invalid_encoding,
           "Non-UTF-8 partition identifier must fail validation");

    records empty_disk_id;
    empty_disk_id.push_back(common::partition_record{});
    empty_disk_id.back().identifier = "sda1";
    empty_disk_id.back().disk_identifier = "";
    const auto empty_disk_res =
        common::validate_partition_records(empty_disk_id);
    expect(!empty_disk_res &&
               empty_disk_res.error() == syscape::errc::invalid_encoding,
           "Empty parent disk identifier must fail validation");

    records contradictory_mount;
    contradictory_mount.push_back(common::partition_record{});
    contradictory_mount.back().identifier = "sda1";
    contradictory_mount.back().disk_identifier = "sda";
    contradictory_mount.back().is_mounted = true;
    const auto contradictory_mount_res =
        common::validate_partition_records(contradictory_mount);
    expect(!contradictory_mount_res &&
               contradictory_mount_res.error() ==
                   syscape::errc::malformed_data,
           "A mounted record without a mount path must be malformed");

    records valid_entry;
    valid_entry.push_back(common::partition_record{});
    valid_entry.back().identifier = "nvme0n1p1";
    valid_entry.back().disk_identifier = "nvme0n1";
    valid_entry.back().partition_number = 1U;
    valid_entry.back().has_name = true;
    valid_entry.back().name = "EFI";
    const auto valid_res =
        common::validate_partition_records(valid_entry);
    expect(valid_res.has_value(),
           "Well-formed partition record must be accepted");
}

void test_live_partitions() {
    const syscape::result<std::vector<syscape::storage::partition_entry>>
        parts = syscape::storage::partitions();
    expect(parts || parts.error() == syscape::errc::not_supported,
           "Partition enumeration must succeed or honestly report not_supported");
    if (!parts) { return; }

    std::string prev_id;
    for (const auto& part : *parts) {
        expect(!part.identifier.empty(),
               "Partition identifier must not be empty");
        expect(!part.disk_identifier.empty(),
               "Partition parent disk identifier must not be empty");
        expect(part.partition_number > 0U,
               "Partition number must be positive");
        expect(prev_id <= part.identifier,
               "Partition list must be sorted by identifier");
        prev_id = part.identifier;

        // Test disk_partitions filter for this drive
        const auto disk_parts =
            syscape::storage::disk_partitions(part.disk_identifier);
        expect(disk_parts.has_value(),
               "disk_partitions must succeed for an existing parent drive");
        if (disk_parts) {
            bool found = false;
            for (const auto& dp : *disk_parts) {
                if (dp.identifier == part.identifier) {
                    found = true;
                    break;
                }
            }
            expect(found,
                   "disk_partitions must contain every partition belonging to that drive");
        }
    }

    const auto invalid_empty = syscape::storage::disk_partitions("");
    expect(!invalid_empty &&
               invalid_empty.error() == syscape::errc::invalid_argument,
           "disk_partitions with empty disk identifier must report invalid_argument");

    const auto invalid_utf8 =
        syscape::storage::disk_partitions("\xff\xfe");
    expect(!invalid_utf8 &&
               invalid_utf8.error() == syscape::errc::invalid_encoding,
           "disk_partitions with invalid UTF-8 must report invalid_encoding");
}

void test_nvme_smart_parser() {
    namespace backend = syscape::detail::storage_backend;
    namespace common = syscape::detail::storage_common;

    std::vector<std::uint8_t> buffer(512U, 0U);
    // Critical warning = 0
    buffer[0] = 0U;
    // Composite temperature: 315 Kelvin (41.85 C)
    buffer[1] = static_cast<std::uint8_t>(315 & 0xFF);
    buffer[2] = static_cast<std::uint8_t>((315 >> 8) & 0xFF);
    // Available spare = 100%
    buffer[3] = 100U;
    // Available spare threshold = 10%
    buffer[4] = 10U;
    // Percentage used = 5%
    buffer[5] = 5U;
    // Data units read = 1000
    buffer[32] = static_cast<std::uint8_t>(1000 & 0xFF);
    buffer[33] = static_cast<std::uint8_t>((1000 >> 8) & 0xFF);
    // Data units written = 500
    buffer[48] = static_cast<std::uint8_t>(500 & 0xFF);
    buffer[49] = static_cast<std::uint8_t>((500 >> 8) & 0xFF);
    // Power cycles = 42
    buffer[112] = 42U;
    // Power on hours = 1234
    buffer[128] = static_cast<std::uint8_t>(1234 & 0xFF);
    buffer[129] = static_cast<std::uint8_t>((1234 >> 8) & 0xFF);
    // Unsafe shutdowns = 3
    buffer[144] = 3U;
    // Media errors = 0
    buffer[160] = 0U;

    common::health_record record;
    record.identifier = "nvme0n1";
    const auto parsed = backend::parse_nvme_smart_log_buffer(
        buffer.data(), buffer.size(), record);
    expect(parsed.has_value(), "Valid 512-byte NVMe SMART buffer must parse successfully");
    expect(record.has_temperature_celsius &&
               record.temperature_celsius > 41.0 &&
               record.temperature_celsius < 42.0,
           "Temperature 315 K must convert to ~41.85 C");
    expect(record.has_available_spare_percent &&
               record.available_spare_percent == 100U,
           "Available spare must be 100%");
    expect(record.has_percent_used && record.percent_used == 5U,
           "Percentage used must be 5%");
    expect(record.has_failure_predicted && !record.failure_predicted,
           "Healthy NVMe drive must report failure_predicted == false");
    expect(record.status == common::health_status_classification::healthy,
           "Healthy NVMe drive must have healthy status");
    expect(record.has_data_units_read_bytes &&
               record.data_units_read_bytes == 1000ULL * 512000ULL,
           "Data units read must convert through 512,000 multiplier");
    expect(record.has_data_units_written_bytes &&
               record.data_units_written_bytes == 500ULL * 512000ULL,
           "Data units written must convert through 512,000 multiplier");
    expect(record.has_power_cycles && record.power_cycles == 42ULL,
           "Power cycles must be 42");
    expect(record.has_power_on_hours && record.power_on_hours == 1234ULL,
           "Power-on hours must be 1234");
    expect(record.has_unsafe_shutdowns && record.unsafe_shutdowns == 3ULL,
           "Unsafe shutdowns must be 3");
    expect(record.has_media_errors && record.media_errors == 0ULL,
           "Media errors must be 0");

    // Test critical warning bit 3 (read-only)
    buffer[0] = 0x08U;
    common::health_record critical_record;
    critical_record.identifier = "nvme0n1";
    expect(backend::parse_nvme_smart_log_buffer(
               buffer.data(), buffer.size(), critical_record).has_value(),
           "Critical warning buffer must parse successfully");
    expect(critical_record.has_failure_predicted && critical_record.failure_predicted,
           "Critical warning must set failure_predicted == true");
    expect(critical_record.status == common::health_status_classification::critical,
           "Critical warning bit 3 must set status == critical");

    // Test warning condition (spare <= threshold)
    buffer[0] = 0U;
    buffer[3] = 5U; // spare = 5 <= thresh = 10
    common::health_record warning_record;
    warning_record.identifier = "nvme0n1";
    expect(backend::parse_nvme_smart_log_buffer(
               buffer.data(), buffer.size(), warning_record).has_value(),
           "Warning condition buffer must parse successfully");
    expect(warning_record.has_failure_predicted && warning_record.failure_predicted,
           "Spare below threshold must set failure_predicted == true");
    expect(warning_record.status == common::health_status_classification::warning,
           "Spare below threshold must set status == warning");

    // Test 128-bit overflow handling: non-zero high 64 bits must return value_too_large
    std::vector<std::uint8_t> overflow_buffer = buffer;
    overflow_buffer[40] = 1U; // High 64 bits of units_read non-zero

    common::health_record overflow_record;
    overflow_record.identifier = "nvme0n1";
    const auto overflow_res = backend::parse_nvme_smart_log_buffer(
        overflow_buffer.data(), overflow_buffer.size(), overflow_record);
    expect(!overflow_res &&
               overflow_res.error() == syscape::errc::value_too_large,
           "128-bit integer overflow in NVMe SMART log must return value_too_large");

    // Test available spare > 100% -> malformed_data
    std::vector<std::uint8_t> bad_spare_buffer = buffer;
    bad_spare_buffer[3] = 105U;
    common::health_record bad_spare_record;
    bad_spare_record.identifier = "nvme0n1";
    const auto bad_spare_res = backend::parse_nvme_smart_log_buffer(
        bad_spare_buffer.data(), bad_spare_buffer.size(), bad_spare_record);
    expect(!bad_spare_res &&
               bad_spare_res.error() == syscape::errc::malformed_data,
           "Available spare > 100% must return malformed_data");

    // Null or truncated buffer
    expect(!backend::parse_nvme_smart_log_buffer(nullptr, 512U, record) &&
               backend::parse_nvme_smart_log_buffer(nullptr, 512U, record).error() ==
                   syscape::errc::malformed_data,
           "Null buffer must fail with malformed_data");
    expect(!backend::parse_nvme_smart_log_buffer(buffer.data(), 511U, record) &&
               backend::parse_nvme_smart_log_buffer(buffer.data(), 511U, record).error() ==
                   syscape::errc::malformed_data,
           "Truncated buffer must fail with malformed_data");
}

void test_ata_smart_parser() {
    namespace backend = syscape::detail::storage_backend;
    namespace common = syscape::detail::storage_common;

    common::health_record normal;
    normal.identifier = "sda";
    const auto parsed_normal =
        backend::parse_ata_smart_status_values(0x4FU, 0xC2U, normal);
    expect(parsed_normal.has_value() && normal.has_failure_predicted &&
               !normal.failure_predicted &&
               normal.status == common::health_status_classification::healthy,
           "ATA normal registers (0x4F, 0xC2) must indicate healthy condition");

    common::health_record failing;
    failing.identifier = "sda";
    const auto parsed_failing =
        backend::parse_ata_smart_status_values(0xF4U, 0x2CU, failing);
    expect(parsed_failing.has_value() && failing.has_failure_predicted &&
               failing.failure_predicted &&
               failing.status == common::health_status_classification::warning,
           "ATA failing registers (0xF4, 0x2C) must indicate warning / threshold exceeded");

    common::health_record unknown;
    unknown.identifier = "sda";
    const auto parsed_unknown =
        backend::parse_ata_smart_status_values(0x00U, 0x00U, unknown);
    expect(!parsed_unknown &&
               parsed_unknown.error() == syscape::errc::malformed_data,
           "Unrecognized register values must return malformed_data");
}

void test_health_boundary_validation() {
    namespace common = syscape::detail::storage_common;

    common::health_record record;
    record.identifier = "nvme0n1";
    record.status = common::health_status_classification::healthy;
    const auto valid = common::validate_health_record(record);
    expect(valid.has_value(), "Valid health record must pass validation");

    common::health_record empty_id;
    empty_id.status = common::health_status_classification::healthy;
    const auto invalid_empty = common::validate_health_record(empty_id);
    expect(!invalid_empty &&
               invalid_empty.error() == syscape::errc::invalid_encoding,
           "Empty identifier must fail validation with invalid_encoding");

    common::health_record bad_utf8;
    bad_utf8.identifier = "\xff\xfe";
    const auto invalid_utf8 = common::validate_health_record(bad_utf8);
    expect(!invalid_utf8 &&
               invalid_utf8.error() == syscape::errc::invalid_encoding,
           "Invalid UTF-8 identifier must fail validation with invalid_encoding");

    common::health_record spare_overflow;
    spare_overflow.identifier = "nvme0n1";
    spare_overflow.has_available_spare_percent = true;
    spare_overflow.available_spare_percent = 105U;
    const auto invalid_spare = common::validate_health_record(spare_overflow);
    expect(!invalid_spare &&
               invalid_spare.error() == syscape::errc::malformed_data,
           "available_spare_percent > 100 must fail with malformed_data");

    common::health_record nan_temp;
    nan_temp.identifier = "nvme0n1";
    nan_temp.has_temperature_celsius = true;
    nan_temp.temperature_celsius = std::numeric_limits<double>::quiet_NaN();
    const auto invalid_temp = common::validate_health_record(nan_temp);
    expect(!invalid_temp &&
               invalid_temp.error() == syscape::errc::malformed_data,
           "NaN temperature must fail with malformed_data");

    expect(!common::is_valid_disk_identifier(""),
           "Empty disk identifier must be invalid");
    expect(!common::is_valid_disk_identifier("."),
           "Dot disk identifier must be invalid");
    expect(!common::is_valid_disk_identifier(".."),
           "Dot-dot disk identifier must be invalid");
    expect(!common::is_valid_disk_identifier("../sda"),
           "Path traversal disk identifier must be invalid");
    expect(!common::is_valid_disk_identifier("/dev/sda"),
           "Slash-containing disk identifier must be invalid");
    expect(!common::is_valid_disk_identifier(std::string_view("sda\0part", 8)),
           "Embedded NUL disk identifier must be invalid");
    expect(common::is_valid_disk_identifier("nvme0n1"),
           "Valid disk name nvme0n1 must be valid");
    expect(common::is_valid_disk_identifier("sda"),
           "Valid disk name sda must be valid");
}

void test_live_drive_health() {
    const auto all_health = syscape::storage::all_drive_health();
    expect(all_health || all_health.error() == syscape::errc::not_supported ||
               all_health.error() == syscape::errc::permission_denied,
           "all_drive_health must succeed, report not_supported, or report permission_denied");
    if (all_health) {
        std::string prev_id;
        for (const auto& h : *all_health) {
            expect(!h.identifier.empty(), "Drive health identifier must not be empty");
            expect(prev_id <= h.identifier,
                   "all_drive_health records must be sorted by identifier");
            prev_id = h.identifier;

            if (h.has_temperature_celsius) {
                expect(h.temperature_celsius >= -40.0 && h.temperature_celsius <= 120.0,
                       "Operating temperature must be within physical limits");
            }

            // Test single-drive health query
            const auto single = syscape::storage::health(h.identifier);
            expect(single.has_value() ||
                       single.error() == syscape::errc::permission_denied,
                   "health(id) must succeed or report permission_denied for an enumerated drive");
            if (single) {
                expect(single->identifier == h.identifier,
                       "health(id) must match target identifier");
            }
        }
    }

    const auto invalid_empty = syscape::storage::health("");
    expect(!invalid_empty &&
               invalid_empty.error() == syscape::errc::invalid_argument,
           "health with empty identifier must report invalid_argument");

    const auto invalid_traversal =
        syscape::storage::health("../block/nvme0n1");
    expect(!invalid_traversal &&
               invalid_traversal.error() == syscape::errc::invalid_argument,
           "health with path traversal must report invalid_argument");

    const auto invalid_nul =
        syscape::storage::health(std::string_view("nvme0n1\0suffix", 14));
    expect(!invalid_nul &&
               invalid_nul.error() == syscape::errc::invalid_argument,
           "health with embedded NUL must report invalid_argument");

    const auto invalid_utf8 = syscape::storage::health("\xff\xfe");
    expect(!invalid_utf8 &&
               invalid_utf8.error() == syscape::errc::invalid_encoding,
           "health with invalid UTF-8 must report invalid_encoding");

    const auto not_found = syscape::storage::health("nonexistent_drive_99999");
    expect(!not_found && not_found.error() == syscape::errc::not_found,
           "health for nonexistent drive must report not_found");
}

void test_ioctl_status_classifiers() {
    namespace backend = syscape::detail::storage_backend;

    // NVMe ioctl status classification:
    // Success
    expect(backend::classify_linux_nvme_ioctl_status(0, 0).has_value(),
           "NVMe rc == 0 must classify as success");

    // Negative syscall returns (interpreting errno)
    const auto nvme_eacces = backend::classify_linux_nvme_ioctl_status(-1, EACCES);
    expect(!nvme_eacces && nvme_eacces.error() == syscape::errc::permission_denied,
           "NVMe EACCES must classify as permission_denied");

    const auto nvme_eperm = backend::classify_linux_nvme_ioctl_status(-1, EPERM);
    expect(!nvme_eperm && nvme_eperm.error() == syscape::errc::permission_denied,
           "NVMe EPERM must classify as permission_denied");

    const auto nvme_enotty = backend::classify_linux_nvme_ioctl_status(-1, ENOTTY);
    expect(nvme_enotty.has_value(), "NVMe ENOTTY must classify as unsupported capability");

    const auto nvme_eopnotsupp = backend::classify_linux_nvme_ioctl_status(-1, EOPNOTSUPP);
    expect(nvme_eopnotsupp.has_value(), "NVMe EOPNOTSUPP must classify as unsupported capability");

    const auto nvme_enosys = backend::classify_linux_nvme_ioctl_status(-1, ENOSYS);
    expect(nvme_enosys.has_value(), "NVMe ENOSYS must classify as unsupported capability");

    const auto nvme_einval = backend::classify_linux_nvme_ioctl_status(-1, EINVAL);
    expect(!nvme_einval && nvme_einval.error() == std::error_code(EINVAL, std::generic_category()),
           "NVMe EINVAL must preserve native invalid argument error");

    const auto nvme_eio = backend::classify_linux_nvme_ioctl_status(-1, EIO);
    expect(!nvme_eio && nvme_eio.error() == syscape::errc::io_error,
           "NVMe EIO must classify as io_error");

    // Positive controller status codes
    // SCT 0, SC 0x01 (Invalid Opcode) -> unsupported capability
    expect(backend::classify_linux_nvme_ioctl_status(0x0001, 0).has_value(),
           "NVMe SC_INVALID_OPCODE must classify as unsupported capability");

    // SCT 0, SC 0x02 (Invalid Field) -> unsupported capability
    expect(backend::classify_linux_nvme_ioctl_status(0x0002, 0).has_value(),
           "NVMe SC_INVALID_FIELD must classify as unsupported capability");

    // SCT 1, SC 0x09 (Invalid Log Page, 0x0109) -> unsupported capability
    expect(backend::classify_linux_nvme_ioctl_status(0x0109, 0).has_value(),
           "NVMe SC_INVALID_LOG_PAGE must classify as unsupported capability");

    // SCT 0, SC 0x03 (Command ID Conflict) -> io_error (NOT permission_denied)
    const auto nvme_cmdid_conflict = backend::classify_linux_nvme_ioctl_status(0x0003, 0);
    expect(!nvme_cmdid_conflict && nvme_cmdid_conflict.error() == syscape::errc::io_error,
           "NVMe SC_CMDID_CONFLICT must classify as io_error");

    // SCT 0, SC 0x06 (Internal Error) -> io_error
    const auto nvme_internal_err = backend::classify_linux_nvme_ioctl_status(0x0006, 0);
    expect(!nvme_internal_err && nvme_internal_err.error() == syscape::errc::io_error,
           "NVMe SC_INTERNAL must classify as io_error");

    // SCSI SG_IO status classification
    backend::linux_sg_io_hdr hdr{};
    hdr.host_status = 0;
    hdr.driver_status = 0;
    hdr.status = 0;

    expect(backend::classify_linux_scsi_ioctl_status(0, 0, hdr, true).has_value(),
           "SCSI with ATA descriptor found must classify as success");

    // ATA descriptor found with driver_status == DRIVER_SENSE (0x08)
    hdr.driver_status = 0x08;
    hdr.status = 0x02; // SAM_STAT_CHECK_CONDITION
    expect(backend::classify_linux_scsi_ioctl_status(0, 0, hdr, true).has_value(),
           "SCSI CHECK_CONDITION + DRIVER_SENSE with ATA descriptor must classify as success");

    // ATA descriptor found but host transmission failed
    hdr.host_status = 1;
    const auto scsi_host_with_desc = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, true);
    expect(!scsi_host_with_desc && scsi_host_with_desc.error() == syscape::errc::io_error,
           "SCSI host error with ATA descriptor must still classify as io_error");

    // ATA descriptor found but unknown driver error
    hdr.host_status = 0;
    hdr.driver_status = 1;
    const auto scsi_driver_err = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, true);
    expect(!scsi_driver_err && scsi_driver_err.error() == syscape::errc::io_error,
           "SCSI driver error with ATA descriptor must classify as io_error");

    // Device status BUSY (0x08) or TASK_SET_FULL (0x28)
    hdr.driver_status = 0;
    hdr.status = 0x08;
    const auto scsi_busy = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, true);
    expect(!scsi_busy && scsi_busy.error() == syscape::errc::temporarily_unavailable,
           "SCSI BUSY status must classify as temporarily_unavailable");

    hdr.status = 0x28;
    const auto scsi_task_set_full = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, true);
    expect(!scsi_task_set_full && scsi_task_set_full.error() == syscape::errc::temporarily_unavailable,
           "SCSI TASK_SET_FULL status must classify as temporarily_unavailable");

    // Reservation conflict (0x18)
    hdr.status = 0x18;
    const auto scsi_res_conflict = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, true);
    expect(!scsi_res_conflict && scsi_res_conflict.error() == syscape::errc::permission_denied,
           "SCSI RESERVATION_CONFLICT must classify as permission_denied");

    // Other unexpected SCSI device status (e.g. 0x04)
    hdr.status = 0x04;
    const auto scsi_other_status = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, true);
    expect(!scsi_other_status && scsi_other_status.error() == syscape::errc::io_error,
           "Unexpected SCSI status must classify as io_error");

    // Negative syscall
    hdr.status = 0;
    const auto scsi_einval = backend::classify_linux_scsi_ioctl_status(-1, EINVAL, hdr, false);
    expect(!scsi_einval && scsi_einval.error() == std::error_code(EINVAL, std::generic_category()),
           "SCSI EINVAL must preserve native invalid argument error");

    const auto scsi_enotty = backend::classify_linux_scsi_ioctl_status(-1, ENOTTY, hdr, false);
    expect(scsi_enotty.has_value(), "SCSI ENOTTY must classify as unsupported capability");

    // No ATA descriptor and host error
    hdr.host_status = 1;
    const auto scsi_host_err = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, false);
    expect(!scsi_host_err && scsi_host_err.error() == syscape::errc::io_error,
           "SCSI host error without ATA descriptor must classify as io_error");

    // Sense data parsing when status == CHECK CONDITION (0x02) without ATA descriptor:
    hdr.host_status = 0;
    hdr.driver_status = 0x08;
    hdr.status = 0x02;

    // 1. Descriptor format sense with ILLEGAL REQUEST (0x05) -> unsupported -> success/empty
    unsigned char sense_desc_illegal[8]{0x72, 0x05, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00};
    hdr.sbp = sense_desc_illegal;
    hdr.sb_len_wr = sizeof(sense_desc_illegal);
    const auto scsi_illegal = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, false);
    expect(scsi_illegal.has_value(),
           "SCSI CHECK_CONDITION with ILLEGAL REQUEST sense must classify as unsupported");

    // 2. Descriptor format sense with NOT READY (0x02) -> temporarily_unavailable
    unsigned char sense_desc_not_ready[8]{0x72, 0x02, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00};
    hdr.sbp = sense_desc_not_ready;
    hdr.sb_len_wr = sizeof(sense_desc_not_ready);
    const auto scsi_not_ready = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, false);
    expect(!scsi_not_ready && scsi_not_ready.error() == syscape::errc::temporarily_unavailable,
           "SCSI CHECK_CONDITION with NOT READY sense must classify as temporarily_unavailable");

    // 3. Descriptor format sense with UNIT ATTENTION (0x06) -> temporarily_unavailable
    unsigned char sense_desc_unit_att[8]{0x72, 0x06, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00};
    hdr.sbp = sense_desc_unit_att;
    hdr.sb_len_wr = sizeof(sense_desc_unit_att);
    const auto scsi_unit_att = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, false);
    expect(!scsi_unit_att && scsi_unit_att.error() == syscape::errc::temporarily_unavailable,
           "SCSI CHECK_CONDITION with UNIT ATTENTION sense must classify as temporarily_unavailable");

    // 4. Descriptor format sense with DATA PROTECT (0x07) -> permission_denied
    unsigned char sense_desc_data_prot[8]{0x72, 0x07, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00};
    hdr.sbp = sense_desc_data_prot;
    hdr.sb_len_wr = sizeof(sense_desc_data_prot);
    const auto scsi_data_prot = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, false);
    expect(!scsi_data_prot && scsi_data_prot.error() == syscape::errc::permission_denied,
           "SCSI CHECK_CONDITION with DATA PROTECT sense must classify as permission_denied");

    // 5. Descriptor format sense with HARDWARE ERROR (0x04) -> io_error
    unsigned char sense_desc_hw_err[8]{0x72, 0x04, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00};
    hdr.sbp = sense_desc_hw_err;
    hdr.sb_len_wr = sizeof(sense_desc_hw_err);
    const auto scsi_hw_err = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, false);
    expect(!scsi_hw_err && scsi_hw_err.error() == syscape::errc::io_error,
           "SCSI CHECK_CONDITION with HARDWARE ERROR sense must classify as io_error");

    // 6. Descriptor format sense with MEDIUM ERROR (0x03) -> io_error
    unsigned char sense_desc_med_err[8]{0x72, 0x03, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00};
    hdr.sbp = sense_desc_med_err;
    hdr.sb_len_wr = sizeof(sense_desc_med_err);
    const auto scsi_med_err = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, false);
    expect(!scsi_med_err && scsi_med_err.error() == syscape::errc::io_error,
           "SCSI CHECK_CONDITION with MEDIUM ERROR sense must classify as io_error");

    // 7. Fixed format sense with HARDWARE ERROR (0x04) -> io_error
    unsigned char sense_fixed_hw_err[14]{0x70, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x44, 0x00};
    hdr.sbp = sense_fixed_hw_err;
    hdr.sb_len_wr = sizeof(sense_fixed_hw_err);
    const auto scsi_fixed_hw = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, false);
    expect(!scsi_fixed_hw && scsi_fixed_hw.error() == syscape::errc::io_error,
           "SCSI CHECK_CONDITION with Fixed Format HARDWARE ERROR must classify as io_error");

    // 8. CHECK_CONDITION but no sense bytes returned (sb_len_wr == 0) -> io_error
    hdr.sbp = nullptr;
    hdr.sb_len_wr = 0;
    const auto scsi_no_sense = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, false);
    expect(!scsi_no_sense && scsi_no_sense.error() == syscape::errc::io_error,
           "SCSI CHECK_CONDITION with empty sense data must classify as io_error");

    // 9. GOOD status (0x00) without descriptor -> success / unsupported
    hdr.status = 0x00;
    const auto scsi_good_no_desc = backend::classify_linux_scsi_ioctl_status(0, 0, hdr, false);
    expect(scsi_good_no_desc.has_value(),
           "SCSI GOOD status without descriptor must classify as success");
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
    test_partition_scheme_classifier();
    test_mount_field_decoder();
    test_partition_boundary_validation();
    test_live_partitions();
    test_nvme_smart_parser();
    test_ata_smart_parser();
    test_health_boundary_validation();
    test_ioctl_status_classifiers();
    test_live_drive_health();
    return failures == 0 ? 0 : 1;
}
