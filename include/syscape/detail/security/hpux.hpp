#ifndef SYSCAPE_DETAIL_SECURITY_HPUX_HPP
#define SYSCAPE_DETAIL_SECURITY_HPUX_HPP

#include <string>
#include <string_view>
#include <vector>

#include <syscape/detail/security/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace security_backend {

inline result<::syscape::security::secure_boot_state> secure_boot() {
    return fail(errc::not_supported);
}

inline result<bool> is_secure_boot_enabled() {
    return fail(errc::not_supported);
}

inline result<::syscape::security::tpm_info> tpm() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> security_modules() {
    return fail(errc::not_supported);
}

inline result<::syscape::security::lockdown_mode> lockdown() {
    return fail(errc::not_supported);
}

inline result<bool> is_sip_enabled() {
    return fail(errc::not_supported);
}

inline result<::syscape::security::aslr_mode> aslr() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::security::cpu_vulnerability_entry>>
cpu_vulnerabilities() {
    return fail(errc::not_supported);
}

inline result<::syscape::security::process_capabilities> capabilities() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::security::privilege_entry>> privileges() {
    return fail(errc::not_supported);
}

inline result<::syscape::security::volume_encryption_info>
volume_encryption(std::string_view) {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::security::volume_encryption_info>>
encrypted_volumes() {
    return fail(errc::not_supported);
}

} // namespace security_backend

} // namespace detail
} // namespace syscape

#endif
