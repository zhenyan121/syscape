#ifndef SYSCAPE_DETAIL_SECURITY_MACOS_HPP
#define SYSCAPE_DETAIL_SECURITY_MACOS_HPP

#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace security_backend {

/// Queries the Apple Secure Boot enablement state on macOS.
inline result<::syscape::security::secure_boot_state> secure_boot() {
    return fail(errc::not_supported);
}

/// Queries whether Secure Boot is active on macOS.
inline result<bool> is_secure_boot_enabled() {
    const auto res = secure_boot();
    if (!res) {
        return fail(res.error());
    }
    return *res == ::syscape::security::secure_boot_state::enabled;
}

/// Queries Trusted Platform Module (TPM) on macOS.
/// Note: Apple platforms use the Apple Secure Enclave (SEP) rather than standard TPM.
inline result<::syscape::security::tpm_info> tpm() {
    ::syscape::security::tpm_info info;
    info.present = false;
    info.version = ::syscape::security::tpm_version::none;
    return info;
}

/// Linux Security Modules are not supported on macOS.
inline result<std::vector<std::string>> security_modules() {
    return fail(errc::not_supported);
}

/// Linux kernel lockdown is not supported on macOS.
inline result<::syscape::security::lockdown_mode> lockdown() {
    return fail(errc::not_supported);
}

/// Queries whether macOS System Integrity Protection (SIP) is enabled.
inline result<bool> is_sip_enabled() {
    return fail(errc::not_supported);
}

/// Queries the Address Space Layout Randomization (ASLR) mode on macOS.
inline result<::syscape::security::aslr_mode> aslr() {
    return fail(errc::not_supported);
}

/// CPU hardware vulnerability table is not exposed on macOS via public APIs.
inline result<std::vector<::syscape::security::cpu_vulnerability_entry>>
cpu_vulnerabilities() {
    return fail(errc::not_supported);
}

/// POSIX capabilities are not supported on macOS.
inline result<::syscape::security::process_capabilities> capabilities() {
    return fail(errc::not_supported);
}

/// Process privileges are not supported on macOS without proprietary entitlement inspection.
inline result<std::vector<::syscape::security::privilege_entry>> privileges() {
    return fail(errc::not_supported);
}

/// Volume encryption queries on macOS require APFS/DiskArbitration APIs which are currently not supported.
inline result<::syscape::security::volume_encryption_info>
volume_encryption(std::string_view path) {
    if (path.empty()) {
        return fail(errc::invalid_argument);
    }
    if (!is_valid_utf8(path)) {
        return fail(errc::invalid_encoding);
    }
    if (path.find('\0') != std::string_view::npos) {
        return fail(errc::invalid_argument);
    }
    return fail(errc::not_supported);
}

/// Volume encryption enumeration on macOS requires APFS/DiskArbitration APIs which are currently not supported.
inline result<std::vector<::syscape::security::volume_encryption_info>>
encrypted_volumes() {
    return fail(errc::not_supported);
}

} // namespace security_backend
} // namespace detail
} // namespace syscape

#endif
