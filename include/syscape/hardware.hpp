#ifndef SYSCAPE_HARDWARE_HPP
#define SYSCAPE_HARDWARE_HPP

/// @file
/// @brief Hosted hardware identity, chassis, firmware, and machine-UUID
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note This first hardware slice exposes the platform-recorded identity of
/// the system, its motherboard, and its firmware, the chassis form factor,
/// and the firmware-recorded hardware UUID. A documented device inventory
/// remains a later slice, and serial numbers stay outside this slice.
/// @note Linux implements the queries through the kernel's documented sysfs
/// DMI-id interface under /sys/class/dmi/id. The kernel documents that
/// interface in its testing ABI classification rather than its stable ABI
/// classification, so future kernels may evolve the rendered attributes.
/// Machines whose firmware provides no DMI records expose no such directory,
/// and every query then reports not_supported. Native failures while probing
/// that directory are preserved. The product_uuid attribute is readable by
/// the privileged account only, so unprivileged callers receive the native
/// permission failure instead of an absent value.
/// @note Windows implements the queries through the documented
/// GetSystemFirmwareTable('RSMB') interface and parses the returned raw
/// SMBIOS structures according to the public DMTF SMBIOS specification.
/// Duplicate singleton system or BIOS records are malformed. Multi-board
/// machines use the hosting-board and board-type fields to identify the
/// motherboard, whose chassis handle selects the system enclosure; an
/// ambiguous topology reports not_supported instead of guessing by record
/// order. The call lives in Kernel32 and needs no additional import library.
/// UUID fields use the version-dependent byte ordering recorded by the raw
/// table header, including the SMBIOS 2.6 little-endian clarification.
/// @note macOS implements the queries through the documented IOKit registry
/// properties of the platform-expert device, enforcing their documented
/// CFData or CFString representation at that boundary. Darwin exposes no
/// publicly documented firmware-version or chassis-classification source
/// reachable there, so those queries report not_supported. Callers must link
/// the IOKit and CoreFoundation frameworks.
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
#include <string>

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

} // namespace hardware
} // namespace syscape

#include <syscape/detail/hardware/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/hardware/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/hardware/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/hardware/macos.hpp>
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

} // namespace hardware
} // namespace syscape

#endif
