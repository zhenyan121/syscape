#ifndef SYSCAPE_DETAIL_SECURITY_MACOS_HPP
#define SYSCAPE_DETAIL_SECURITY_MACOS_HPP

#include <string>
#include <vector>

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

} // namespace security_backend
} // namespace detail
} // namespace syscape

#endif
