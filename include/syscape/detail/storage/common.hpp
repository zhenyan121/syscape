#ifndef SYSCAPE_DETAIL_STORAGE_COMMON_HPP
#define SYSCAPE_DETAIL_STORAGE_COMMON_HPP

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace storage_common {

using bus_classification = ::syscape::storage::bus_type;
using partition_scheme_classification = ::syscape::storage::partition_scheme;
using health_status_classification = ::syscape::storage::drive_health_status;

/// One recorded health and SMART diagnostics snapshot shared by the Hosted backends.
struct health_record {
    /// Verbatim platform label of the whole-disk device.
    std::string identifier;
    /// Overall health status classification.
    health_status_classification status = health_status_classification::unknown;
    /// Whether failure prediction / SMART threshold trip was recorded.
    bool has_failure_predicted = false;
    /// Whether failure is predicted by drive firmware.
    bool failure_predicted = false;
    /// Whether operating temperature is recorded.
    bool has_temperature_celsius = false;
    /// Current drive temperature in degrees Celsius.
    double temperature_celsius = 0.0;
    /// Whether endurance / wear level indicator is recorded.
    bool has_percent_used = false;
    /// Whole percentage of drive life used (0-100%, can exceed 100% for over-worn SSDs).
    std::uint32_t percent_used = 0U;
    /// Whether NVMe available spare capacity is recorded.
    bool has_available_spare_percent = false;
    /// Normalized remaining spare capacity percentage (0-100%).
    std::uint32_t available_spare_percent = 0U;
    /// Whether power-on hours is recorded.
    bool has_power_on_hours = false;
    /// Cumulative power-on hours.
    std::uint64_t power_on_hours = 0U;
    /// Whether power cycle count is recorded.
    bool has_power_cycles = false;
    /// Cumulative power cycles.
    std::uint64_t power_cycles = 0U;
    /// Whether unsafe shutdowns count is recorded.
    bool has_unsafe_shutdowns = false;
    /// Cumulative unsafe shutdowns.
    std::uint64_t unsafe_shutdowns = 0U;
    /// Whether media error count is recorded.
    bool has_media_errors = false;
    /// Total media read/write errors.
    std::uint64_t media_errors = 0U;
    /// Whether data units read total is recorded.
    bool has_data_units_read_bytes = false;
    /// Total bytes read from device.
    std::uint64_t data_units_read_bytes = 0U;
    /// Whether data units written total is recorded.
    bool has_data_units_written_bytes = false;
    /// Total bytes written to device.
    std::uint64_t data_units_written_bytes = 0U;
};

/// One recorded whole-disk snapshot shared by the Hosted backends awaiting
/// boundary conversion.
struct drive_record {
    /// Verbatim platform label of the whole-disk device.
    std::string identifier;
    /// Whether the platform exposed a vendor string.
    bool has_vendor = false;
    /// Vendor string rendered by the platform, meaningful only when
    /// has_vendor is true.
    std::string vendor;
    /// Whether the platform exposed a model string.
    bool has_model = false;
    /// Model string rendered by the platform, meaningful only when has_model
    /// is true.
    std::string model;
    /// Whether the platform exposed a firmware revision.
    bool has_firmware_revision = false;
    /// Firmware revision rendered by the platform, meaningful only when
    /// has_firmware_revision is true.
    std::string firmware_revision;
    /// The platform's recorded transport classification.
    bus_classification bus = bus_classification::unknown;
    /// Whether the platform exposed a total capacity.
    bool has_capacity_bytes = false;
    /// Total device capacity in bytes. Zero is valid data that describes a
    /// device without media.
    std::uint64_t capacity_bytes = 0U;
    /// Whether the platform exposed the logical block size.
    bool has_logical_sector_size_bytes = false;
    /// Logical block size in bytes, meaningful only when the corresponding
    /// flag is true.
    std::uint32_t logical_sector_size_bytes = 0U;
    /// Whether the platform exposed the physical block size.
    bool has_physical_sector_size_bytes = false;
    /// Physical block size in bytes, meaningful only when the corresponding
    /// flag is true.
    std::uint32_t physical_sector_size_bytes = 0U;
    /// Whether the platform records whether the medium rotates. Platforms
    /// that expose no recorded rotation fact leave this flag false instead
    /// of guessing from performance characteristics.
    bool has_rotational = false;
    /// Whether the medium rotates, meaningful only when has_rotational is
    /// true.
    bool rotational = false;
    /// Whether the platform reports the medium as removable or ejectable.
    bool removable = false;
};

/// One recorded partition snapshot shared by the Hosted backends awaiting
/// boundary conversion.
struct partition_record {
    /// Verbatim platform partition device identifier.
    std::string identifier;
    /// Parent drive identifier.
    std::string disk_identifier;
    /// 1-based partition index on the disk.
    std::uint32_t partition_number = 0U;
    /// Whether start offset is recorded.
    bool has_start_offset_bytes = false;
    /// Starting byte offset on the parent disk.
    std::uint64_t start_offset_bytes = 0U;
    /// Whether size is recorded.
    bool has_size_bytes = false;
    /// Partition capacity in bytes.
    std::uint64_t size_bytes = 0U;
    /// Partition table scheme.
    partition_scheme_classification scheme =
        partition_scheme_classification::unknown;
    /// Whether a partition type identifier is recorded.
    bool has_type_identifier = false;
    /// Partition type identifier string.
    std::string type_identifier;
    /// Whether a partition name is recorded.
    bool has_name = false;
    /// Partition name or label.
    std::string name;
    /// Whether a partition UUID is recorded.
    bool has_uuid = false;
    /// Partition unique identifier or UUID.
    std::string uuid;
    /// Whether a filesystem type is recorded.
    bool has_filesystem_type = false;
    /// Filesystem type name.
    std::string filesystem_type;
    /// Whether the partition is read-only.
    bool is_read_only = false;
    /// Whether the partition is active/bootable.
    bool is_bootable = false;
    /// Whether the partition is mounted.
    bool is_mounted = false;
    /// Mount path if mounted.
    std::string mount_point;
};

/// Validates converted drive entries at the public boundary.
///
/// Identifiers and every present text field must be well-formed UTF-8,
/// because Hosted Full text is UTF-8 by contract. Present block sizes must
/// be nonzero. They need not be powers of two: some storage protocols expose
/// formats such as 520-byte logical blocks. A capacity may be zero because
/// devices without media record zero rather than an error sentinel.
inline result<std::vector<drive_record>> validate_drive_records(
    result<std::vector<drive_record>> records) {
    if (!records) { return fail(records.error()); }
    for (const drive_record& record : *records) {
        if (record.identifier.empty() || !is_valid_utf8(record.identifier)) {
            return fail(errc::invalid_encoding);
        }
        if ((record.has_vendor && !is_valid_utf8(record.vendor)) ||
            (record.has_model && !is_valid_utf8(record.model)) ||
            (record.has_firmware_revision &&
             !is_valid_utf8(record.firmware_revision))) {
            return fail(errc::invalid_encoding);
        }
        if ((record.has_logical_sector_size_bytes &&
             record.logical_sector_size_bytes == 0U) ||
            (record.has_physical_sector_size_bytes &&
             record.physical_sector_size_bytes == 0U)) {
            return fail(errc::malformed_data);
        }
    }
    return records;
}

/// Validates converted partition entries at the public boundary.
inline result<std::vector<partition_record>> validate_partition_records(
    result<std::vector<partition_record>> records) {
    if (!records) { return fail(records.error()); }
    for (const partition_record& record : *records) {
        if (record.identifier.empty() || !is_valid_utf8(record.identifier)) {
            return fail(errc::invalid_encoding);
        }
        if (record.disk_identifier.empty() ||
            !is_valid_utf8(record.disk_identifier)) {
            return fail(errc::invalid_encoding);
        }
        if ((record.has_type_identifier && record.type_identifier.empty()) ||
            (record.has_name && record.name.empty()) ||
            (record.has_uuid && record.uuid.empty()) ||
            (record.has_filesystem_type && record.filesystem_type.empty()) ||
            (record.is_mounted && record.mount_point.empty()) ||
            (!record.is_mounted && !record.mount_point.empty())) {
            return fail(errc::malformed_data);
        }
        if ((record.has_type_identifier &&
             !is_valid_utf8(record.type_identifier)) ||
            (record.has_name && !is_valid_utf8(record.name)) ||
            (record.has_uuid && !is_valid_utf8(record.uuid)) ||
            (record.has_filesystem_type &&
             !is_valid_utf8(record.filesystem_type)) ||
            (record.is_mounted && !is_valid_utf8(record.mount_point))) {
            return fail(errc::invalid_encoding);
        }
    }
    return records;
}

/// Validates converted health entry at the public boundary.
inline result<health_record> validate_health_record(
    result<health_record> record) {
    if (!record) { return fail(record.error()); }
    if (record->identifier.empty() || !is_valid_utf8(record->identifier)) {
        return fail(errc::invalid_encoding);
    }
    if (record->has_available_spare_percent &&
        record->available_spare_percent > 100U) {
        return fail(errc::malformed_data);
    }
    if (record->has_temperature_celsius) {
        if (std::isnan(record->temperature_celsius) ||
            std::isinf(record->temperature_celsius) ||
            record->temperature_celsius < -273.15 ||
            record->temperature_celsius > 1000.0) {
            return fail(errc::malformed_data);
        }
    }
    return record;
}

/// Validates converted health entries at the public boundary.
inline result<std::vector<health_record>> validate_health_records(
    result<std::vector<health_record>> records) {
    if (!records) { return fail(records.error()); }
    for (const health_record& record : *records) {
        if (record.identifier.empty() || !is_valid_utf8(record.identifier)) {
            return fail(errc::invalid_encoding);
        }
        if (record.has_available_spare_percent &&
            record.available_spare_percent > 100U) {
            return fail(errc::malformed_data);
        }
        if (record.has_temperature_celsius) {
            if (std::isnan(record.temperature_celsius) ||
                std::isinf(record.temperature_celsius) ||
                record.temperature_celsius < -273.15 ||
                record.temperature_celsius > 1000.0) {
                return fail(errc::malformed_data);
            }
        }
    }
    return records;
}

/// Checks that a caller-supplied disk identifier is structurally valid
/// (non-empty, valid UTF-8, no embedded NUL, no path traversal components).
inline bool is_valid_disk_identifier(std::string_view identifier) noexcept {
    if (identifier.empty()) { return false; }
    if (!is_valid_utf8(identifier)) { return false; }
    if (identifier.find('\0') != std::string_view::npos) { return false; }
    if (identifier.find('/') != std::string_view::npos ||
        identifier.find('\\') != std::string_view::npos) {
        return false;
    }
    if (identifier == "." || identifier == "..") { return false; }
    return true;
}

} // namespace storage_common
} // namespace detail
} // namespace syscape

#endif
