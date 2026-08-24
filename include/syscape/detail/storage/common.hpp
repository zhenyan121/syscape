#ifndef SYSCAPE_DETAIL_STORAGE_COMMON_HPP
#define SYSCAPE_DETAIL_STORAGE_COMMON_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace storage_common {

using bus_classification = ::syscape::storage::bus_type;

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

} // namespace storage_common
} // namespace detail
} // namespace syscape

#endif
