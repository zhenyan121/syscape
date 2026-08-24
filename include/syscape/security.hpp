#ifndef SYSCAPE_SECURITY_HPP
#define SYSCAPE_SECURITY_HPP

/// @file
/// @brief Hosted security, Secure Boot, TPM, LSM, and system integrity queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note This module exposes:
/// - UEFI / platform Secure Boot enablement state (secure_boot(), is_secure_boot_enabled()).
/// - Trusted Platform Module presence and specification version (tpm()).
/// - Active Linux Security Modules and kernel security frameworks (security_modules()).
/// - Linux kernel lockdown protection level (lockdown()).
/// - macOS System Integrity Protection status (is_sip_enabled()).
/// @note Linux queries sysfs efivars (/sys/firmware/efi/efivars), sysfs TPM (/sys/class/tpm),
/// and securityfs (/sys/kernel/security).
/// @note Windows queries GetFirmwareEnvironmentVariableW and dynamically loads
/// the system TPM Base Services API. Firmware-variable access can require the
/// SE_SYSTEM_ENVIRONMENT_NAME privilege.
/// @note macOS currently reports not_supported for Secure Boot and SIP because
/// no stable public process API provides the required state.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/security.hpp requires C++17 or later"
#endif

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace syscape {
namespace security {

/// State of UEFI or platform Secure Boot on the system.
enum class secure_boot_state : std::uint8_t {
    /// The Secure Boot state could not be determined.
    unknown,
    /// Secure Boot is supported and currently enabled and enforcing.
    enabled,
    /// Secure Boot is supported but currently disabled or not enforcing.
    disabled,
    /// Secure Boot enforcement is inactive because the firmware is in audit or setup mode.
    audit
};

/// TPM (Trusted Platform Module) specification version.
enum class tpm_version : std::uint8_t {
    /// The TPM version is unknown or could not be determined.
    unknown,
    /// No TPM device is detected on the system.
    none,
    /// TPM 1.2 specification.
    v1_2,
    /// TPM 2.0 specification.
    v2_0,
    /// Other or unclassified TPM version.
    other
};

/// Linux kernel lockdown protection level.
enum class lockdown_mode : std::uint8_t {
    /// Lockdown state uses a newer mode not recognized by this library version.
    unknown,
    /// Kernel lockdown is disabled / none.
    none,
    /// Integrity lockdown mode is active.
    integrity,
    /// Confidentiality lockdown mode is active.
    confidentiality
};

/// Information describing an installed Trusted Platform Module (TPM).
struct tpm_info {
    /// Whether a TPM hardware or firmware interface is detected.
    bool present = false;

    /// TPM specification version enum.
    tpm_version version = tpm_version::unknown;

    /// Verbatim version string (e.g. "2.0", "1.2", "2"), if exposed.
    std::string version_string;

    /// TPM device interface or node identifier (e.g. "tpm0"), if exposed.
    std::optional<std::string> device_id;

    /// TPM manufacturer or driver description if exposed.
    std::optional<std::string> description;
};

} // namespace security
} // namespace syscape

#include <syscape/detail/security/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/security/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/security/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/security/macos.hpp>
#else
#include <syscape/detail/security/generic.hpp>
#endif

namespace syscape {
namespace security {

/// Queries the UEFI or platform Secure Boot enablement state.
///
/// The value is fixed for the current boot. Linux requires the documented EFI
/// sysfs interfaces. Windows firmware access can require
/// SE_SYSTEM_ENVIRONMENT_NAME. macOS currently reports not_supported.
///
/// @return The secure_boot_state; not_supported when the platform or firmware
/// does not expose the state; permission_denied when access is denied;
/// malformed_data for invalid platform data; otherwise a native read error.
inline result<secure_boot_state> secure_boot() {
    return detail::security_backend::secure_boot();
}

/// Queries whether Secure Boot is active and enforcing.
///
/// The value is fixed for the current boot and has the same availability and
/// error behavior as secure_boot().
///
/// @return true if Secure Boot is enabled, false if explicitly disabled or in
/// audit/setup mode, or an error when the state cannot be determined.
inline result<bool> is_secure_boot_enabled() {
    return detail::security_backend::is_secure_boot_enabled();
}

/// Queries Trusted Platform Module (TPM) presence and specification version.
///
/// Device presence can change when the platform supports removable or virtual
/// TPM interfaces. A detected device can have an unknown version when the OS
/// does not expose it.
///
/// @return A tpm_info structure with detected facts; permission_denied,
/// temporarily_unavailable, io_error, invalid_encoding for non-UTF-8 platform
/// text, or a native platform error on failure.
inline result<tpm_info> tpm() {
    return detail::security_backend::tpm();
}

/// Queries the list of active kernel security modules (e.g. Linux LSMs).
///
/// The list is queried on demand and can change after a reboot or platform
/// reconfiguration. Linux uses the authoritative securityfs LSM list.
///
/// @return A vector of active module names (e.g. "landlock", "lockdown", "yama", "bpf"),
/// not_supported when no authoritative interface exists, malformed_data for an
/// invalid list, invalid_encoding for non-UTF-8 names, or a native access error.
inline result<std::vector<std::string>> security_modules() {
    return detail::security_backend::security_modules();
}

/// Queries the Linux kernel lockdown protection level.
///
/// The value is queried on demand and normally remains fixed for the boot.
///
/// @return The lockdown_mode; not_supported when the kernel exposes no
/// lockdown interface, malformed_data for invalid contents, or a native access
/// error.
inline result<lockdown_mode> lockdown() {
    return detail::security_backend::lockdown();
}

/// Queries whether macOS System Integrity Protection (SIP) is enabled.
///
/// @return true if SIP is active, false if disabled, or not_supported when no
/// stable public process API provides the state. The current backends return
/// not_supported.
inline result<bool> is_sip_enabled() {
    return detail::security_backend::is_sip_enabled();
}

} // namespace security
} // namespace syscape

#endif
