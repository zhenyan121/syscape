#ifndef SYSCAPE_HARDWARE_HPP
#define SYSCAPE_HARDWARE_HPP

/// @file
/// @brief Hosted hardware identity, chassis, firmware, machine-UUID, and
/// device inventory queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms, Android, and OpenHarmony).
/// @note Apple mobile platforms provide manufacturer and model information
/// from public sysctl values. Chassis classification, firmware version and
/// release date, hardware UUIDs, and PCI, USB, and memory device inventories
/// report not_supported.
/// @note This module exposes the platform-recorded identity of the system,
/// its motherboard, and its firmware, the chassis form factor, the
/// firmware-recorded hardware UUID, as well as hardware device inventory:
/// PCI/PCIe devices, USB bus devices, and physical memory slots/modules.
/// @note Linux implements identity queries through sysfs DMI-id under
/// /sys/class/dmi/id. PCI devices are enumerated by scanning
/// /sys/bus/pci/devices/. USB devices are enumerated by scanning
/// /sys/bus/usb/devices/. Physical memory modules are parsed from the raw
/// SMBIOS structure table at /sys/firmware/dmi/tables/DMI when readable.
/// @note Windows implements identity and memory module queries through
/// GetSystemFirmwareTable('RSMB') parsing DMTF SMBIOS tables. PCI and USB
/// devices are enumerated via SetupAPI device discovery and property queries.
/// Callers using those inventory queries must link Setupapi.lib.
/// @note macOS implements identity and device queries via IOKit
/// registry properties (IOPlatformExpertDevice, IOPCIDevice, IOUSBDevice).
/// Physical memory-module enumeration has no verified public source there and
/// reports not_supported. Callers must link IOKit and CoreFoundation.
/// @note Haiku reports not_supported for system hardware identity and bus
/// device inventories because the platform documents no public in-process C
/// hardware inventory API.
/// @note AIX reports system manufacturer as "IBM"; model names, versions,
/// UUID, and bus device inventories report not_supported.
/// @note hardware_uuid() exposes a machine identifier. The query is explicit,
/// preserves permission failures, performs no logging, persistence, or
/// network access, and reports the SMBIOS-documented absence renderings as
/// not_found. Firmware settings, motherboard replacement, and reimaging can
/// change or clear the value, so it must not be presented as a permanent
/// identity guarantee.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/hardware.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace syscape {
namespace hardware {

/// Recorded chassis form-factor classification at the moment of a query.
///
/// Every implemented source renders the SMBIOS System Enclosure Type
/// vocabulary: Linux publishes the recorded chassis-type integer verbatim,
/// and Windows parses the identical byte from the raw SMBIOS enclosure
/// record. The classification therefore describes the firmware's own record
/// and stays comparable between those platforms rather than being invented
/// by Syscape. macOS exposes no documented public chassis source, so its
/// query reports not_supported. A firmware that records no recognizable
/// classification reports unknown, which is data about the record rather
/// than an error.
enum class form_factor : std::uint8_t {
    /// The firmware records no usable classification (SMBIOS type 2).
    unknown,
    /// A classified form factor outside this vocabulary (SMBIOS type 1).
    other,
    desktop,
    low_profile_desktop,
    pizza_box,
    mini_tower,
    tower,
    portable,
    laptop,
    notebook,
    hand_held,
    docking_station,
    all_in_one,
    sub_notebook,
    space_saving,
    lunch_box,
    main_server,
    expansion_chassis,
    sub_chassis,
    bus_expansion_chassis,
    peripheral_chassis,
    raid_chassis,
    rack_mount_chassis,
    sealed_case_pc,
    multi_system,
    compact_pci,
    advanced_tca,
    blade,
    blade_enclosure,
    tablet,
    convertible,
    detachable,
    iot_gateway,
    embedded_pc,
    mini_pc,
    stick_pc
};

/// PCI device base class classification according to PCI-SIG specification.
enum class pci_class : std::uint8_t {
    unknown,
    unclassified,
    mass_storage,
    network_controller,
    display_controller,
    multimedia_controller,
    memory_controller,
    bridge,
    communication_controller,
    generic_system_peripheral,
    input_device_controller,
    docking_station,
    processor,
    serial_bus_controller,
    wireless_controller,
    intelligent_controller,
    satellite_communication,
    encryption_controller,
    signal_processing_controller,
    processing_accelerator,
    non_essential_instrumentation
};

/// Physical memory module form factor according to SMBIOS Type 17 specification.
enum class memory_form_factor : std::uint8_t {
    unknown,
    other,
    simm,
    sip,
    chip,
    dip,
    zip,
    soj,
    proprietary,
    dimm,
    tsop,
    row_of_chips,
    rimm,
    sodimm,
    srimm,
    fb_dimm,
    die,
    camm
};

/// Memory technology and standard according to SMBIOS Type 17 specification.
enum class memory_type : std::uint8_t {
    unknown,
    other,
    dram,
    edram,
    vram,
    sram,
    ram,
    rom,
    flash,
    eeprom,
    feprom,
    eprom,
    cdram,
    three_d_ram,
    sdram,
    sgram,
    rdram,
    ddr,
    ddr2,
    ddr2_fb_dimm,
    ddr3,
    fbd2,
    ddr4,
    lpddr,
    lpddr2,
    lpddr3,
    lpddr4,
    logical_non_volatile,
    hbm,
    hbm2,
    ddr5,
    lpddr5,
    hbm3
};

/// Installation state recorded for one physical memory socket/device.
enum class memory_device_state : std::uint8_t {
    /// Firmware does not identify whether a module is installed.
    unknown,
    /// The physical socket is not populated.
    not_installed,
    /// A memory device is installed, although its capacity can remain unknown.
    installed
};

/// Unit assigned by the SMBIOS revision to a recorded memory speed value.
enum class memory_speed_unit : std::uint8_t {
    /// The table revision is unavailable, so MHz and MT/s cannot be separated.
    unknown,
    /// Megahertz, as specified through SMBIOS 3.0.
    megahertz,
    /// Megatransfers per second, as specified by SMBIOS 3.1 and later.
    megatransfers_per_second
};

/// One firmware-recorded memory speed together with its exact unit.
struct memory_speed {
    /// Numeric speed in the accompanying unit.
    std::uint32_t value{0U};
    /// Unit determined from the SMBIOS table revision.
    memory_speed_unit unit{memory_speed_unit::unknown};
};

/// PCI/PCIe device description.
struct pci_device {
    /// PCI domain / segment number, if the backend exposes it.
    std::optional<std::uint16_t> domain{};
    /// PCI bus number, if the backend exposes it.
    std::optional<std::uint8_t> bus{};
    /// PCI device number on the bus, if the backend exposes it.
    std::optional<std::uint8_t> device{};
    /// PCI function number on the device, if the backend exposes it.
    std::optional<std::uint8_t> function{};
    /// PCI 16-bit vendor identifier assigned by PCI-SIG.
    std::uint16_t vendor_id{0};
    /// PCI 16-bit device identifier assigned by the vendor.
    std::uint16_t device_id{0};
    /// Optional PCI subsystem vendor identifier.
    std::optional<std::uint16_t> subsystem_vendor_id{};
    /// Optional PCI subsystem device identifier.
    std::optional<std::uint16_t> subsystem_device_id{};
    /// PCI base class code, if exposed (e.g. 0x03 for display controller).
    std::optional<std::uint8_t> class_code{};
    /// PCI subclass code, if exposed (e.g. 0x00 for VGA-compatible).
    std::optional<std::uint8_t> subclass_code{};
    /// PCI programming interface register code, if exposed.
    std::optional<std::uint8_t> programming_interface{};
    /// Portable PCI base class classification.
    pci_class device_class{pci_class::unknown};
    /// Bound kernel driver or service name, if bound.
    std::optional<std::string> driver{};
    /// Physical or ACPI slot identifier string, if exposed.
    std::optional<std::string> slot_name{};
};

/// USB bus device description.
struct usb_device {
    /// Host USB bus controller number, if exposed.
    std::optional<std::uint8_t> bus_number{};
    /// Device address assigned on the bus, if exposed.
    std::optional<std::uint8_t> device_address{};
    /// Hub port number on the upstream hub, if exposed.
    std::optional<std::uint8_t> port_number{};
    /// USB 16-bit vendor identifier assigned by USB-IF.
    std::uint16_t vendor_id{0};
    /// USB 16-bit product identifier assigned by the vendor.
    std::uint16_t product_id{0};
    /// Device release number in binary-coded decimal, if exposed.
    std::optional<std::uint16_t> bcd_device{};
    /// USB device class code, if exposed.
    std::optional<std::uint8_t> device_class{};
    /// USB device subclass code, if exposed.
    std::optional<std::uint8_t> device_subclass{};
    /// USB device protocol code, if exposed.
    std::optional<std::uint8_t> device_protocol{};
    /// Manufacturer string descriptor in UTF-8, if exposed.
    std::optional<std::string> manufacturer{};
    /// Product string descriptor in UTF-8, if exposed.
    std::optional<std::string> product{};
    /// Serial number string descriptor in UTF-8, if exposed.
    std::optional<std::string> serial_number{};
    /// Operating signaling speed in megabits per second (e.g. 480, 5000, 10000).
    std::optional<double> speed_mbps{};
};

/// Physical memory slot and installed memory module record (SMBIOS Type 17).
struct memory_device {
    /// Socket or board slot label (e.g. "DIMM 0", "ChannelA-DIMM0").
    std::string locator{};
    /// Bank or channel label (e.g. "BANK 0"), if exposed.
    std::optional<std::string> bank_locator{};
    /// Whether the slot is populated according to firmware.
    memory_device_state state{memory_device_state::unknown};
    /// Installed memory module capacity in bytes, if firmware records it.
    std::optional<std::uint64_t> size_bytes{};
    /// Physical form factor of the module or socket.
    memory_form_factor form_factor{memory_form_factor::unknown};
    /// Memory technology and generation.
    memory_type type{memory_type::unknown};
    /// Manufacturer-rated speed and its SMBIOS-defined unit, if exposed.
    std::optional<memory_speed> speed{};
    /// Configured/operating speed and its SMBIOS-defined unit, if exposed.
    std::optional<memory_speed> configured_speed{};
    /// Memory module manufacturer string in UTF-8, if exposed.
    std::optional<std::string> manufacturer{};
    /// Memory module serial number string in UTF-8, if exposed.
    std::optional<std::string> serial_number{};
    /// Memory module manufacturer part number string in UTF-8, if exposed.
    std::optional<std::string> part_number{};
};

} // namespace hardware
} // namespace syscape

#include <syscape/detail/hardware/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__) && !defined(SYSCAPE_TARGET_OPENHARMONY) &&           \
    !defined(SYSCAPE_TARGET_AIX)
#include <syscape/detail/hardware/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/hardware/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/hardware/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/hardware/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/hardware/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/hardware/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/hardware/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/hardware/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/hardware/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/hardware/openharmony.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/hardware/solaris.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__HAIKU__)
#include <syscape/detail/hardware/haiku.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_AIX)
#include <syscape/detail/hardware/aix.hpp>
#else
#include <syscape/detail/hardware/generic.hpp>
#endif

namespace syscape {
namespace hardware {

/// Returns the system manufacturer recorded by the firmware as UTF-8.
///
/// The value describes the party the firmware records as the system vendor,
/// which can differ from the party that manufactured individual components.
/// Every query observes the firmware record afresh, so changes require a
/// reboot or firmware reconfiguration to become visible.
/// @return The manufacturer, not_found when the platform records none, or an
/// error such as not_supported on platforms without a usable source.
inline result<std::string> system_manufacturer() {
    return detail::hardware_common::validate_identity_text(
        detail::hardware_backend::system_manufacturer());
}

/// Returns the system product name recorded by the firmware as UTF-8.
/// @return The name, not_found when the platform records none, or an error.
inline result<std::string> system_product_name() {
    return detail::hardware_common::validate_identity_text(
        detail::hardware_backend::system_product_name());
}

/// Returns the system product version recorded by the firmware as UTF-8.
///
/// Platforms record different kinds of versions here, such as a model
/// revision or a configuration code, so the value is comparable within one
/// platform only.
/// @return The version, not_found when the platform records none, or an
/// error.
inline result<std::string> system_product_version() {
    return detail::hardware_common::validate_identity_text(
        detail::hardware_backend::system_product_version());
}

/// Returns the motherboard manufacturer recorded by the platform as UTF-8.
/// @return The manufacturer, not_found when the platform records none, or an
/// error such as not_supported on platforms without a usable source.
inline result<std::string> motherboard_manufacturer() {
    return detail::hardware_common::validate_identity_text(
        detail::hardware_backend::motherboard_manufacturer());
}

/// Returns the motherboard product name recorded by the platform as UTF-8.
/// @return The name, not_found when the platform records none, or an error.
inline result<std::string> motherboard_product_name() {
    return detail::hardware_common::validate_identity_text(
        detail::hardware_backend::motherboard_product_name());
}

/// Returns the motherboard version recorded by the platform as UTF-8.
/// @return The version, not_found when the platform records none, or an
/// error.
inline result<std::string> motherboard_version() {
    return detail::hardware_common::validate_identity_text(
        detail::hardware_backend::motherboard_version());
}

/// Returns the firmware or BIOS vendor recorded by the platform as UTF-8.
/// @return The vendor, not_found when the platform records none, or an error
/// such as not_supported on platforms without a usable source.
inline result<std::string> firmware_vendor() {
    return detail::hardware_common::validate_identity_text(
        detail::hardware_backend::firmware_vendor());
}

/// Returns the firmware or BIOS version recorded by the platform as UTF-8.
/// @return The version, not_found when the platform records none, or an
/// error.
inline result<std::string> firmware_version() {
    return detail::hardware_common::validate_identity_text(
        detail::hardware_backend::firmware_version());
}

/// Returns the firmware or BIOS release date recorded by the platform as
/// UTF-8.
///
/// The rendering is reported verbatim. Both implemented firmware sources
/// follow the SMBIOS convention of month, day, and four-digit year separated
/// by slashes, but the contract preserves whatever the platform records.
/// @return The release date, not_found when the platform records none, or an
/// error.
inline result<std::string> firmware_release_date() {
    return detail::hardware_common::validate_identity_text(
        detail::hardware_backend::firmware_release_date());
}

/// Returns the chassis form factor recorded by the firmware.
///
/// See form_factor for the meaning and comparability of the vocabulary. The
/// classification changes only when firmware settings or the physical
/// enclosure change, so repeated calls agree between reboots.
/// @return The recorded classification, not_found when the platform records
/// none, or not_supported on platforms without a documented source.
inline result<form_factor> chassis_form_factor() {
    return detail::hardware_backend::chassis_form_factor();
}

/// Returns the firmware-recorded hardware UUID in the canonical lowercase
/// hexadecimal rendering, for example 03000200-0400-0500-0006-000700080009.
///
/// The identifier distinguishes machines within fleets and inventories, so
/// this query exists separately from the descriptive identity queries and
/// carries privacy implications: treat the value as personal or
/// asset-tracking data, query it explicitly, and never log or persist it
/// incidentally. The SMBIOS specification defines the all-zero and all-one
/// byte renderings as recording no identifier, and this query reports
/// not_found for them instead of returning a meaningless value. Firmware
/// updates, motherboard replacement, and reimaging can change or clear the
/// value, so callers must not present it as a permanent identity guarantee.
/// On Linux the underlying attribute is readable by the privileged account
/// only, so unprivileged callers preserve the native permission failure.
/// @return The canonical rendering, not_found when the firmware records no
/// identifier, a native permission error where access is restricted, or
/// not_supported on platforms without a usable source.
inline result<std::string> hardware_uuid() {
    return detail::hardware_common::validate_uuid_text(
        detail::hardware_backend::hardware_uuid());
}

/// Returns all observable PCI and PCIe devices on the system.
///
/// Address components absent from a platform source remain empty. Devices are
/// sorted deterministically by address and then identity fields.
/// @return Vector of PCI devices; malformed source data, native discovery
/// failures, and invalid text encodings are reported as errors.
inline result<std::vector<pci_device>> pci_devices() {
    return detail::hardware_common::validate_pci_devices(
        detail::hardware_backend::pci_devices());
}

/// Returns all connected USB bus devices on the system.
///
/// Optional topology and descriptor fields remain empty when the platform does
/// not expose them. Devices are sorted deterministically by topology and
/// identity fields.
/// @return Vector of USB devices; malformed source data, native discovery
/// failures, and invalid text encodings are reported as errors.
inline result<std::vector<usb_device>> usb_devices() {
    return detail::hardware_common::validate_usb_devices(
        detail::hardware_backend::usb_devices());
}

/// Returns physical memory slots and installed memory modules (SMBIOS Type 17).
///
/// The state field distinguishes an unpopulated slot from an installed module
/// whose capacity is unknown. Records are sorted by slot and bank locator.
/// @return Vector of memory slots/devices, or an error such as permission_denied
/// or not_supported.
inline result<std::vector<memory_device>> memory_devices() {
    return detail::hardware_common::validate_memory_devices(
        detail::hardware_backend::memory_devices());
}

} // namespace hardware
} // namespace syscape

#endif
