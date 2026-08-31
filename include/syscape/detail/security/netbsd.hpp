#ifndef SYSCAPE_DETAIL_SECURITY_NETBSD_HPP
#define SYSCAPE_DETAIL_SECURITY_NETBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <system_error>
#include <vector>

#include <syscape/security.hpp>
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
    int enabled = 0;
    std::size_t size = sizeof(enabled);
    if (::sysctlbyname("security.pax.aslr.enabled", &enabled, &size, nullptr,
                       0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(enabled)) {
        return fail(errc::malformed_data);
    }
    return enabled != 0 ? ::syscape::security::aslr_mode::full
                        : ::syscape::security::aslr_mode::disabled;
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
