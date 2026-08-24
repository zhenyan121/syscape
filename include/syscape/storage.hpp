#ifndef SYSCAPE_STORAGE_HPP
#define SYSCAPE_STORAGE_HPP

/// @file
/// @brief Hosted physical-drive enumeration queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note This first storage slice enumerates whole-disk block devices with
/// their identity strings, transport classification, capacity, block sizes,
/// rotation, and removability. Partition tables and operating-system health
/// reporting remain outside this slice.
/// @note Linux implements the query through the kernel's documented sysfs
/// block interface under /sys/block. The kernel assigns disks to transport
/// subsystems rather than to controller protocols, so a SATA or USB disk
/// behind the SCSI layer records its subsystem classification; constructs
/// without a backing device node such as loop, device-mapper, and zram
/// devices are excluded because they are software constructs rather than
/// recorded drives. Windows implements the query through the documented
/// storage stack: \\.\PhysicalDriveN device names,
/// SetupAPI disk-device-interface enumeration,
/// IOCTL_STORAGE_GET_DEVICE_NUMBER identity records,
/// IOCTL_STORAGE_QUERY_PROPERTY descriptor records, and
/// IOCTL_DISK_GET_DRIVE_GEOMETRY_EX. The platform exposes no recorded
/// rotation fact through those interfaces, so rotation stays unreported
/// instead of being inferred from performance properties, and virtual-bus
/// disks that the platform itself records as physical drives are enumerated.
/// macOS implements the query through the documented DiskArbitration
/// description interface (declared in <DiskArbitration/DiskArbitration.h>)
/// together with IOKit media registry entries, excluding image-backed media
/// whose protocol the platform records as virtual. The platform exposes no
/// physical-block-size or rotation record through these interfaces, so
/// those fields stay unreported there. Other targets use the not-supported
/// fallback.
/// @note Windows callers that use drives() must link Setupapi.lib.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/storage.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>
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

} // namespace storage
} // namespace syscape

#include <syscape/detail/storage/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/storage/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/storage/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/storage/macos.hpp>
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

/// Returns one entry per whole-disk drive recorded by the platform.
///
/// Entries are ordered by ascending identifier so an unchanged population
/// always enumerates identically. Only whole-disk devices satisfy this
/// contract; partitions of those disks belong to a later slice. Software
/// constructs that no backing hardware device node supports, such as loop,
/// device-mapper, memory-backed, and compressed-RAM devices, are excluded
/// on Linux, while file-backed disks that the platform itself records in
/// its physical-drive namespace are enumerated on Windows. An empty vector
/// is valid data that means the platform records no qualifying drive.
/// Every field can change between calls as drives are connected, removed,
/// powered down, or loaded with different media.
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

} // namespace storage
} // namespace syscape

#endif
