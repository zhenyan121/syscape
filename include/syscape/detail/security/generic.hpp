#ifndef SYSCAPE_DETAIL_SECURITY_GENERIC_HPP
#define SYSCAPE_DETAIL_SECURITY_GENERIC_HPP

#include <string>
#include <vector>

#include <syscape/error.hpp>
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

} // namespace security_backend
} // namespace detail
} // namespace syscape

#endif
