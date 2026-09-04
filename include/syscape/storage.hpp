#ifndef SYSCAPE_STORAGE_HPP
#define SYSCAPE_STORAGE_HPP

/// @file
/// @brief Hosted physical-drive, partition, and drive health / SMART queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms, Android, and OpenHarmony).
/// @note Apple mobile platforms expose no permitted public physical-drive,
/// partition, or SMART inventory source to this C++ interface, so all queries
/// report not_supported.
/// @note This storage module enumerates whole-disk block devices with
/// their identity strings, transport classification, capacity, block sizes,
/// rotation, removability, partition layout information, and hardware health /
/// SMART diagnostics.
/// @note Linux implements drive and partition queries through the kernel's
/// documented sysfs block interface under /sys/block and /proc/mounts, and
/// health diagnostics through NVMe Admin ioctls, SCSI/ATA pass-through ioctls,
/// and hwmon temperature sensors.
/// Windows implements queries through the storage stack: \\.\PhysicalDriveN
/// device names, SetupAPI disk-device-interface enumeration,
/// IOCTL_STORAGE_GET_DEVICE_NUMBER, IOCTL_STORAGE_QUERY_PROPERTY,
/// IOCTL_STORAGE_PREDICT_FAILURE, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
/// IOCTL_DISK_GET_DRIVE_LAYOUT_EX, volume enumeration, and volume disk extents.
/// macOS implements queries through the DiskArbitration framework and
/// IOKit media registry entries, resolving partition media to qualifying
/// non-virtual whole disks, and reading SMART Status and IORegistry statistics.
/// Android implements drive queries through the sysfs block interface under
/// /sys/block. AIX and HP-UX report not_supported for physical block-device and
/// SMART queries. Other targets use the not-supported fallback.
/// @note Windows callers that use drives(), partitions(), or health queries
/// must link Setupapi.lib.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/storage.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace syscape {
namespace storage {

/// Recorded transport classification of one drive at the moment of a
/// query.
///
/// Every value reports the platform's own recorded classification rather
/// than an inferred controller protocol, so identifiers are comparable
/// within one platform and deliberately not normalized across platforms:
/// Linux maps the kernel-recorded subsystem of the backing device, where a
/// disk behind the SCSI layer records scsi regardless of whether its
/// controller is SATA, SAS, or USB-attached; Windows renders its documented
/// bus-type constants, which do distinguish SATA from SAS from USB;
/// macOS maps its recorded device-protocol descriptions. A transport the
/// platform does not classify into this vocabulary records unknown instead
/// of the nearest-looking value.
enum class bus_type : std::uint8_t {
    /// The platform exposes no usable transport classification.
    unknown,
    /// The platform classifies the drive as generic SCSI.
    scsi,
    /// The platform classifies the drive as SATA or PATA.
    sata,
    /// The platform classifies the drive as SAS.
    sas,
    /// The platform classifies the drive as ATA without further precision.
    ata,
    /// The platform classifies the drive as ATAPI.
    atapi,
    /// The platform classifies the drive as USB-attached.
    usb,
    /// The platform classifies the drive as FireWire-attached.
    firewire,
    /// The platform classifies the drive as Fibre Channel-attached.
    fibre_channel,
    /// The platform classifies the drive as iSCSI-attached.
    iscsi,
    /// The platform classifies the drive as behind a RAID controller or
    /// container without exposing member transports.
    raid,
    /// The platform classifies the drive as NVMe-attached.
    nvme,
    /// The platform classifies the drive as SD-card attached.
    sd,
    /// The platform classifies the drive as MMC or eMMC attached.
    mmc,
    /// The platform classifies the drive as file-backed or otherwise
    /// virtual media that it nevertheless records as a drive.
    virtual_media
};

/// Partition table / partitioning scheme classification.
enum class partition_scheme : std::uint8_t {
    /// The platform exposes no usable partition scheme or the scheme is unrecognized.
    unknown,
    /// Master Boot Record (MBR / DOS) partitioning scheme.
    mbr,
    /// GUID Partition Table (GPT) partitioning scheme.
    gpt,
    /// Apple Partition Map (APM) scheme.
    apple,
    /// Unpartitioned raw whole-disk format.
    raw
};

/// Recorded health classification of a physical storage drive.
enum class drive_health_status : std::uint8_t {
    /// Health status is unknown or not reported by the platform.
    unknown,
    /// Drive reports normal, healthy operating condition (SMART passed / no failure predicted).
    healthy,
    /// Drive reports degraded condition or pre-fail warning threshold exceeded.
    warning,
    /// Drive reports critical failure or severe unrecoverable condition.
    critical
};

} // namespace storage
} // namespace syscape

#include <syscape/detail/storage/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__) && !defined(SYSCAPE_TARGET_OPENHARMONY) &&           \
    !defined(SYSCAPE_TARGET_AIX) && !defined(SYSCAPE_TARGET_HPUX)
#include <syscape/detail/storage/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/storage/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/storage/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/storage/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/storage/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/storage/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/storage/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/storage/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/storage/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/storage/openharmony.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/storage/solaris.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__HAIKU__)
#include <syscape/detail/storage/haiku.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_AIX)
#include <syscape/detail/storage/aix.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_HPUX)
#include <syscape/detail/storage/hpux.hpp>
#else
#include <syscape/detail/storage/generic.hpp>
#endif

namespace syscape {
namespace storage {

/// One whole-disk drive snapshot reported by the platform.
struct drive_entry {
    /// Verbatim platform label rendered as UTF-8, for example the kernel
    /// device name on Linux or the PhysicalDrive index rendering on
    /// Windows. The label identifies the device within one running system;
    /// device enumeration order is not stable across reboots or hardware
    /// changes.
    std::string identifier;
    /// Whether the platform exposed a vendor string for this drive.
    bool has_vendor;
    /// Vendor string rendered by the platform, meaningful only when
    /// has_vendor is true. Firmware renderings may include padding, which
    /// is trimmed.
    std::string vendor;
    /// Whether the platform exposed a model string for this drive.
    bool has_model;
    /// Model string rendered by the platform, meaningful only when
    /// has_model is true.
    std::string model;
    /// Whether the platform exposed a firmware revision.
    bool has_firmware_revision;
    /// Firmware revision rendered by the platform, meaningful only when
    /// has_firmware_revision is true.
    std::string firmware_revision;
    /// The platform's recorded transport classification.
    bus_type bus;
    /// Whether the platform exposed a total capacity for this drive.
    bool has_capacity_bytes;
    /// Total device capacity in bytes, meaningful only when
    /// has_capacity_bytes is true. Zero is valid data that describes a
    /// device currently holding no medium, which is ordinary for empty
    /// optical or card-reader devices. The value changes when removable
    /// media change.
    std::uint64_t capacity_bytes;
    /// Whether the platform exposed the logical block size.
    bool has_logical_sector_size_bytes;
    /// Logical block size in bytes, meaningful only when the corresponding
    /// flag is true. This is the smallest addressable unit the platform
    /// accepts for reads and writes.
    std::uint32_t logical_sector_size_bytes;
    /// Whether the platform exposed the physical block size.
    bool has_physical_sector_size_bytes;
    /// Physical block size in bytes, meaningful only when the corresponding
    /// flag is true. This is the platform-reported physical unit used for
    /// storage alignment and atomicity, and it can exceed the logical size.
    std::uint32_t physical_sector_size_bytes;
    /// Whether the platform records whether the medium rotates. Platforms
    /// whose public interfaces expose no recorded rotation fact leave this
    /// flag false instead of deriving an answer from performance traits.
    bool has_rotational;
    /// Whether the medium rotates, meaningful only when has_rotational is
    /// true.
    bool rotational;
    /// Whether the platform reports the medium as removable or ejectable.
    bool removable;
};

/// One storage partition snapshot reported by the platform.
struct partition_entry {
    /// Verbatim platform partition device identifier rendered as UTF-8
    /// (e.g. "nvme0n1p1", "sda1", "PhysicalDrive0Partition1", "disk0s1").
    std::string identifier;
    /// Identifier of the parent physical drive (e.g. "nvme0n1", "sda", "PhysicalDrive0", "disk0").
    std::string disk_identifier;
    /// 1-based partition index on the disk as recorded by the platform (or 0 if unnumbered).
    std::uint32_t partition_number = 0U;
    /// Whether the platform exposed a start offset for this partition.
    bool has_start_offset_bytes = false;
    /// Byte offset of the partition from the start of the parent drive.
    std::uint64_t start_offset_bytes = 0U;
    /// Whether the platform exposed a total size for this partition.
    bool has_size_bytes = false;
    /// Total partition capacity in bytes.
    std::uint64_t size_bytes = 0U;
    /// Partitioning scheme of the partition table.
    partition_scheme scheme = partition_scheme::unknown;
    /// Whether the platform exposed a partition type identifier (e.g. GPT type GUID or MBR type hex).
    bool has_type_identifier = false;
    /// Partition type identifier string rendered verbatim (e.g. GPT type GUID or MBR type hex like "0x83").
    std::string type_identifier;
    /// Whether the platform exposed a partition name / label (e.g. GPT partition label).
    bool has_name = false;
    /// Partition name / label rendered by the platform.
    std::string name;
    /// Whether the platform exposed a partition unique identifier / UUID (e.g. GPT PartitionId GUID or PARTUUID).
    bool has_uuid = false;
    /// Partition UUID string rendered by the platform.
    std::string uuid;
    /// Whether the platform exposed a filesystem type.
    bool has_filesystem_type = false;
    /// Filesystem type name (e.g. "ext4", "btrfs", "ntfs", "apfs", "vfat").
    std::string filesystem_type;
    /// Whether the platform records the partition as read-only.
    bool is_read_only = false;
    /// Whether the platform records the partition as active / bootable.
    bool is_bootable = false;
    /// Whether the partition is currently mounted.
    bool is_mounted = false;
    /// Mount point path if currently mounted, meaningful only when is_mounted is true.
    std::string mount_point;
};

/// One physical storage drive health and SMART diagnostics snapshot reported by the platform.
struct drive_health {
    /// Verbatim platform disk identifier (e.g. "nvme0n1", "sda", "PhysicalDrive0", "disk0").
    std::string identifier;
    /// Overall health status classification.
    drive_health_status status = drive_health_status::unknown;
    /// Whether failure prediction / SMART threshold trip status was recorded.
    bool has_failure_predicted = false;
    /// Whether impending drive failure is predicted by firmware.
    bool failure_predicted = false;
    /// Whether current operating temperature is recorded.
    bool has_temperature_celsius = false;
    /// Current drive temperature in degrees Celsius (°C).
    double temperature_celsius = 0.0;
    /// Whether endurance / wear level indicator is recorded.
    bool has_percent_used = false;
    /// Estimate of drive life used as a whole percentage (0-100%, can exceed 100% for over-worn SSDs).
    std::uint32_t percent_used = 0U;
    /// Whether NVMe available spare capacity is recorded.
    bool has_available_spare_percent = false;
    /// Normalized remaining spare capacity percentage (0-100%).
    std::uint32_t available_spare_percent = 0U;
    /// Whether total power-on hours is recorded.
    bool has_power_on_hours = false;
    /// Cumulative power-on time in hours.
    std::uint64_t power_on_hours = 0U;
    /// Whether power cycle count is recorded.
    bool has_power_cycles = false;
    /// Cumulative power cycle count.
    std::uint64_t power_cycles = 0U;
    /// Whether unexpected / unsafe power loss count is recorded.
    bool has_unsafe_shutdowns = false;
    /// Cumulative unsafe shutdowns count.
    std::uint64_t unsafe_shutdowns = 0U;
    /// Whether uncorrectable media read/write errors are recorded.
    bool has_media_errors = false;
    /// Total unrecovered read/write error occurrences reported by drive.
    std::uint64_t media_errors = 0U;
    /// Whether cumulative data read total is recorded.
    bool has_data_units_read_bytes = false;
    /// Total data read from the device in bytes.
    std::uint64_t data_units_read_bytes = 0U;
    /// Whether cumulative data written total is recorded.
    bool has_data_units_written_bytes = false;
    /// Total data written to the device in bytes.
    std::uint64_t data_units_written_bytes = 0U;
};

/// Returns one entry per whole-disk drive recorded by the platform.
///
/// Entries are ordered by ascending identifier so an unchanged population
/// always enumerates identically. Software constructs that no backing hardware
/// device node supports, such as loop, device-mapper, memory-backed, and
/// compressed-RAM devices, are excluded on Linux, while file-backed disks
/// that the platform itself records in its physical-drive namespace are
/// enumerated on Windows. An empty vector is valid data that means the
/// platform records no qualifying drive.
/// @return Zero or more entries, malformed_data for contradictory platform
/// data, invalid_encoding for unconvertible labels, or a native platform
/// error.
inline result<std::vector<drive_entry>> drives() {
    const result<std::vector<detail::storage_common::drive_record>> records =
        detail::storage_common::validate_drive_records(
            detail::storage_backend::drives());
    if (!records) { return fail(records.error()); }
    std::vector<drive_entry> output;
    output.reserve(records->size());
    for (const detail::storage_common::drive_record& record : *records) {
        drive_entry entry;
        entry.identifier = record.identifier;
        entry.has_vendor = record.has_vendor;
        entry.vendor = record.vendor;
        entry.has_model = record.has_model;
        entry.model = record.model;
        entry.has_firmware_revision = record.has_firmware_revision;
        entry.firmware_revision = record.firmware_revision;
        entry.bus = record.bus;
        entry.has_capacity_bytes = record.has_capacity_bytes;
        entry.capacity_bytes = record.capacity_bytes;
        entry.has_logical_sector_size_bytes =
            record.has_logical_sector_size_bytes;
        entry.logical_sector_size_bytes = record.logical_sector_size_bytes;
        entry.has_physical_sector_size_bytes =
            record.has_physical_sector_size_bytes;
        entry.physical_sector_size_bytes = record.physical_sector_size_bytes;
        entry.has_rotational = record.has_rotational;
        entry.rotational = record.rotational;
        entry.removable = record.removable;
        output.push_back(std::move(entry));
    }
    return output;
}

/// Returns one entry per storage partition recorded by the platform across all drives.
///
/// Entries are sorted by ascending identifier.
/// @return Zero or more partition entries, malformed_data for contradictory platform data,
/// invalid_encoding for unconvertible text, or a native platform error.
inline result<std::vector<partition_entry>> partitions() {
    const result<std::vector<detail::storage_common::partition_record>> records =
        detail::storage_common::validate_partition_records(
            detail::storage_backend::partitions());
    if (!records) { return fail(records.error()); }
    std::vector<partition_entry> output;
    output.reserve(records->size());
    for (const detail::storage_common::partition_record& record : *records) {
        partition_entry entry;
        entry.identifier = record.identifier;
        entry.disk_identifier = record.disk_identifier;
        entry.partition_number = record.partition_number;
        entry.has_start_offset_bytes = record.has_start_offset_bytes;
        entry.start_offset_bytes = record.start_offset_bytes;
        entry.has_size_bytes = record.has_size_bytes;
        entry.size_bytes = record.size_bytes;
        entry.scheme = record.scheme;
        entry.has_type_identifier = record.has_type_identifier;
        entry.type_identifier = record.type_identifier;
        entry.has_name = record.has_name;
        entry.name = record.name;
        entry.has_uuid = record.has_uuid;
        entry.uuid = record.uuid;
        entry.has_filesystem_type = record.has_filesystem_type;
        entry.filesystem_type = record.filesystem_type;
        entry.is_read_only = record.is_read_only;
        entry.is_bootable = record.is_bootable;
        entry.is_mounted = record.is_mounted;
        entry.mount_point = record.mount_point;
        output.push_back(std::move(entry));
    }
    return output;
}

/// Returns partition entries belonging to the specified physical drive identifier.
///
/// \param disk_identifier Identifier of the parent drive (e.g. "nvme0n1", "sda", "PhysicalDrive0", "disk0").
/// \return Zero or more partition entries belonging to that drive, or an error.
inline result<std::vector<partition_entry>> disk_partitions(
    std::string_view disk_identifier) {
    if (disk_identifier.empty()) {
        return fail(errc::invalid_argument);
    }
    if (!detail::is_valid_utf8(disk_identifier)) {
        return fail(errc::invalid_encoding);
    }
    if (disk_identifier.find('\0') != std::string_view::npos ||
        disk_identifier.find('/') != std::string_view::npos ||
        disk_identifier.find('\\') != std::string_view::npos ||
        disk_identifier == "." || disk_identifier == "..") {
        return fail(errc::invalid_argument);
    }
    const result<std::vector<partition_entry>> all = partitions();
    if (!all) { return fail(all.error()); }
    std::vector<partition_entry> matching;
    for (const partition_entry& entry : *all) {
        if (entry.disk_identifier == disk_identifier) {
            matching.push_back(entry);
        }
    }
    return matching;
}

/// Returns health and SMART diagnostics for the specified physical drive identifier.
///
/// \param disk_identifier Identifier of the target physical drive (e.g. "nvme0n1", "sda", "PhysicalDrive0", "disk0").
/// \return A drive_health record, not_found if the drive does not exist, permission_denied if unprivileged,
/// or another native/portable error.
inline result<drive_health> health(std::string_view disk_identifier) {
    if (disk_identifier.empty()) {
        return fail(errc::invalid_argument);
    }
    if (!detail::is_valid_utf8(disk_identifier)) {
        return fail(errc::invalid_encoding);
    }
    if (disk_identifier.find('\0') != std::string_view::npos ||
        disk_identifier.find('/') != std::string_view::npos ||
        disk_identifier.find('\\') != std::string_view::npos ||
        disk_identifier == "." || disk_identifier == "..") {
        return fail(errc::invalid_argument);
    }
    const result<detail::storage_common::health_record> record =
        detail::storage_common::validate_health_record(
            detail::storage_backend::health(disk_identifier));
    if (!record) { return fail(record.error()); }
    drive_health entry;
    entry.identifier = record->identifier;
    entry.status = record->status;
    entry.has_failure_predicted = record->has_failure_predicted;
    entry.failure_predicted = record->failure_predicted;
    entry.has_temperature_celsius = record->has_temperature_celsius;
    entry.temperature_celsius = record->temperature_celsius;
    entry.has_percent_used = record->has_percent_used;
    entry.percent_used = record->percent_used;
    entry.has_available_spare_percent = record->has_available_spare_percent;
    entry.available_spare_percent = record->available_spare_percent;
    entry.has_power_on_hours = record->has_power_on_hours;
    entry.power_on_hours = record->power_on_hours;
    entry.has_power_cycles = record->has_power_cycles;
    entry.power_cycles = record->power_cycles;
    entry.has_unsafe_shutdowns = record->has_unsafe_shutdowns;
    entry.unsafe_shutdowns = record->unsafe_shutdowns;
    entry.has_media_errors = record->has_media_errors;
    entry.media_errors = record->media_errors;
    entry.has_data_units_read_bytes = record->has_data_units_read_bytes;
    entry.data_units_read_bytes = record->data_units_read_bytes;
    entry.has_data_units_written_bytes = record->has_data_units_written_bytes;
    entry.data_units_written_bytes = record->data_units_written_bytes;
    return entry;
}

/// Returns health and SMART diagnostics for all physical storage drives recorded by the platform.
///
/// \return Vector of drive_health entries sorted by ascending identifier, or an error.
inline result<std::vector<drive_health>> all_drive_health() {
    const result<std::vector<detail::storage_common::health_record>> records =
        detail::storage_common::validate_health_records(
            detail::storage_backend::all_drive_health());
    if (!records) { return fail(records.error()); }
    std::vector<drive_health> output;
    output.reserve(records->size());
    for (const detail::storage_common::health_record& record : *records) {
        drive_health entry;
        entry.identifier = record.identifier;
        entry.status = record.status;
        entry.has_failure_predicted = record.has_failure_predicted;
        entry.failure_predicted = record.failure_predicted;
        entry.has_temperature_celsius = record.has_temperature_celsius;
        entry.temperature_celsius = record.temperature_celsius;
        entry.has_percent_used = record.has_percent_used;
        entry.percent_used = record.percent_used;
        entry.has_available_spare_percent = record.has_available_spare_percent;
        entry.available_spare_percent = record.available_spare_percent;
        entry.has_power_on_hours = record.has_power_on_hours;
        entry.power_on_hours = record.power_on_hours;
        entry.has_power_cycles = record.has_power_cycles;
        entry.power_cycles = record.power_cycles;
        entry.has_unsafe_shutdowns = record.has_unsafe_shutdowns;
        entry.unsafe_shutdowns = record.unsafe_shutdowns;
        entry.has_media_errors = record.has_media_errors;
        entry.media_errors = record.media_errors;
        entry.has_data_units_read_bytes = record.has_data_units_read_bytes;
        entry.data_units_read_bytes = record.data_units_read_bytes;
        entry.has_data_units_written_bytes = record.has_data_units_written_bytes;
        entry.data_units_written_bytes = record.data_units_written_bytes;
        output.push_back(std::move(entry));
    }
    return output;
}

} // namespace storage
} // namespace syscape

#endif
