#ifndef SYSCAPE_SECURITY_HPP
#define SYSCAPE_SECURITY_HPP

/// @file
/// @brief Hosted security, Secure Boot, TPM, LSM, ASLR, vulnerability
/// mitigations, process capabilities, privileges, and volume encryption
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note This module exposes:
/// - UEFI / platform Secure Boot enablement state (secure_boot(),
/// is_secure_boot_enabled()).
/// - Trusted Platform Module presence and specification version (tpm()).
/// - Active Linux Security Modules and kernel security frameworks
/// (security_modules()).
/// - Linux kernel lockdown protection level (lockdown()).
/// - macOS System Integrity Protection status (is_sip_enabled()).
/// - Address Space Layout Randomization policy level (aslr()).
/// - Operating system and CPU hardware vulnerability mitigations
/// (cpu_vulnerabilities()).
/// - POSIX process capabilities across capability sets (capabilities()).
/// - Observable process token privileges and enabled flags (privileges()).
/// - Storage volume and filesystem encryption state visibility
/// (volume_encryption(), encrypted_volumes()).
/// @note Linux queries sysfs efivars (/sys/firmware/efi/efivars), sysfs TPM
/// (/sys/class/tpm), securityfs (/sys/kernel/security),
/// /proc/sys/kernel/randomize_va_space,
/// /sys/devices/system/cpu/vulnerabilities, /proc/self/status, and
/// device-mapper sysfs.
/// @note Windows queries GetFirmwareEnvironmentVariableW, TPM Base Services
/// (tbs.dll), GetProcessMitigationPolicy, OpenProcessToken, and
/// TokenPrivileges.
/// @note Android queries Verified Boot properties and randomize_va_space for
/// ASLR.
/// @note macOS reports full ASLR, SIP status, and fallback security properties.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/security.hpp requires C++17 or later"
#endif

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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

/// Address Space Layout Randomization (ASLR) configuration level.
enum class aslr_mode : std::uint8_t {
    /// ASLR status is unknown or could not be determined.
    unknown,
    /// ASLR is disabled.
    disabled,
    /// ASLR is partially enabled (e.g. stack, mmap, and VDSO randomized, but not brk/data).
    partial,
    /// Full ASLR is active (stack, mmap, VDSO, and brk/data randomized).
    full
};

/// Hardware vulnerability mitigation status classification.
enum class mitigation_status : std::uint8_t {
    /// The mitigation state is unknown or unrecognized.
    unknown,
    /// The hardware or environment is not affected by this vulnerability.
    not_affected,
    /// The vulnerability is actively mitigated by kernel or microcode defenses.
    mitigated,
    /// The system is vulnerable to this attack.
    vulnerable,
    /// Mitigation for this vulnerability has been disabled (e.g. via boot parameters).
    disabled
};

/// Information describing an operating system or CPU hardware vulnerability mitigation.
struct cpu_vulnerability_entry {
    /// Vulnerability identifier (e.g. "meltdown", "spectre_v1", "spectre_v2", "retbleed", "zenbleed").
    std::string name;

    /// Classified mitigation status.
    mitigation_status status = mitigation_status::unknown;

    /// Verbatim description string exposed by the platform.
    std::string raw_description;
};

/// Fine-grained POSIX / OS process capabilities of the calling process.
struct process_capabilities {
    /// Capabilities actively used for permission checks.
    std::vector<std::string> effective;

    /// Capabilities that the process can assume.
    std::vector<std::string> permitted;

    /// Capabilities preserved across an execve.
    std::vector<std::string> inheritable;

    /// Bounding set bounding the capabilities a process may gain.
    std::vector<std::string> bounding;

    /// Ambient capabilities preserved across non-setuid execve.
    std::vector<std::string> ambient;
};

/// Observable privilege entry for the calling process.
struct privilege_entry {
    /// Privilege or capability name (e.g. "cap_sys_admin", "SeDebugPrivilege").
    std::string name;

    /// Whether this privilege is currently enabled/active for the process.
    bool enabled = false;

    /// Whether this privilege is enabled by default in the process token.
    bool enabled_by_default = false;
};

/// Status of filesystem or volume encryption.
enum class encryption_state : std::uint8_t {
    /// Encryption state is unknown or could not be determined.
    unknown,
    /// The volume or filesystem is not encrypted.
    unencrypted,
    /// The volume or filesystem is fully encrypted.
    encrypted,
    /// The volume contains mixed encrypted and unencrypted data (e.g. per-directory fscrypt).
    mixed
};

/// Type or technology used for volume / filesystem encryption.
enum class encryption_type : std::uint8_t {
    /// Encryption technology is unknown or could not be determined.
    unknown,
    /// No encryption is applied.
    none,
    /// Linux Unified Key Setup (LUKS) / dm-crypt.
    luks,
    /// Microsoft BitLocker drive encryption.
    bitlocker,
    /// Apple FileVault / APFS encryption.
    filevault,
    /// Linux filesystem-level encryption (fscrypt).
    fscrypt,
    /// Plain dm-crypt without LUKS header.
    dm_crypt,
    /// Other or unclassified encryption mechanism.
    other
};

/// Information describing the encryption status of a volume or filesystem.
struct volume_encryption_info {
    /// Target path or block device identifier.
    std::string path_or_device;

    /// Encryption state classification.
    encryption_state state = encryption_state::unknown;

    /// Encryption type or technology.
    encryption_type type = encryption_type::unknown;

    /// Cipher algorithm name (e.g. "aes-xts-plain64"), if exposed.
    std::optional<std::string> cipher;

    /// Key size in bits (e.g. 256, 512), if exposed.
    std::optional<std::uint32_t> key_size_bits;
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
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/security/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/security/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/security/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/security/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/security/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/security/solaris.hpp>
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

/// Queries the Address Space Layout Randomization (ASLR) mode.
///
/// Linux reads /proc/sys/kernel/randomize_va_space.
/// Windows queries GetProcessMitigationPolicy(ProcessASLRPolicy).
/// macOS currently reports not_supported.
///
/// @return The aslr_mode enum; not_supported when unavailable,
/// permission_denied when a sandbox or platform policy blocks the source, or
/// another platform read error.
inline result<aslr_mode> aslr() {
    return detail::security_backend::aslr();
}

/// Queries known CPU and platform hardware vulnerability mitigations.
///
/// Linux enumerates /sys/devices/system/cpu/vulnerabilities/.
/// Windows and macOS currently report not_supported.
///
/// @return A vector of cpu_vulnerability_entry structs sorted by name;
/// not_supported when unavailable.
inline result<std::vector<cpu_vulnerability_entry>> cpu_vulnerabilities() {
    return detail::security_backend::cpu_vulnerabilities();
}

/// Queries the fine-grained process capabilities of the calling process.
///
/// Linux decodes /proc/self/status Cap* capability masks.
/// Windows and macOS return not_supported.
///
/// @return A process_capabilities struct; not_supported on non-POSIX/non-Linux systems.
inline result<process_capabilities> capabilities() {
    return detail::security_backend::capabilities();
}

/// Queries the process privileges of the calling process.
///
/// Linux maps effective capabilities into privilege entries.
/// Windows queries OpenProcessToken with TokenPrivileges.
/// macOS returns not_supported.
///
/// @return A vector of privilege_entry structs; not_supported when unavailable.
inline result<std::vector<privilege_entry>> privileges() {
    return detail::security_backend::privileges();
}

/// Queries the encryption status of the volume containing the specified path.
///
/// Linux checks device-mapper block nodes and slave hierarchies (/sys/block/*/dm/uuid).
/// Windows and macOS currently report not_supported until native SDK providers are verified.
///
/// @param path The filesystem path or device path to inspect.
/// @return volume_encryption_info; invalid_argument for empty or invalid paths;
/// not_found if path does not exist; not_supported when unavailable.
inline result<volume_encryption_info> volume_encryption(std::string_view path) {
    return detail::security_backend::volume_encryption(path);
}

/// Queries all observable encrypted storage volumes on the system.
///
/// Linux scans /proc/self/mounts and /sys/block device-mapper entries.
/// Windows and macOS currently report not_supported until native SDK providers are verified.
///
/// @return A vector of volume_encryption_info structs sorted by path_or_device.
inline result<std::vector<volume_encryption_info>> encrypted_volumes() {
    return detail::security_backend::encrypted_volumes();
}

} // namespace security
} // namespace syscape

#endif
