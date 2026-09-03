#ifndef SYSCAPE_DETAIL_SECURITY_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_SECURITY_OPENHARMONY_HPP

#include <cerrno>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/openharmony/file.hpp>
#include <syscape/detail/openharmony/parameter.hpp>
#include <syscape/detail/security/common.hpp>
#include <syscape/result.hpp>
#include <syscape/security.hpp>

namespace syscape {
namespace detail {
namespace security_backend {

inline result<::syscape::security::secure_boot_state> secure_boot() {
    const result<std::string> state =
        openharmony::get_parameter("ro.boot.verifiedbootstate");
    if (state) {
        if (*state == "green" || *state == "yellow") {
            return ::syscape::security::secure_boot_state::enabled;
        }
        if (*state == "orange" || *state == "red") {
            return ::syscape::security::secure_boot_state::disabled;
        }
        return fail(errc::malformed_data);
    }
    if (state.error() != errc::not_found &&
        state.error() != errc::not_supported) {
        return fail(state.error());
    }

    const result<std::string> vbmeta =
        openharmony::get_parameter("ro.boot.vbmeta.device_state");
    if (vbmeta) {
        if (*vbmeta == "locked") {
            return ::syscape::security::secure_boot_state::enabled;
        }
        if (*vbmeta == "unlocked") {
            return ::syscape::security::secure_boot_state::disabled;
        }
        return fail(errc::malformed_data);
    }
    if (vbmeta.error() != errc::not_found &&
        vbmeta.error() != errc::not_supported) {
        return fail(vbmeta.error());
    }

    return fail(errc::not_supported);
}

inline result<bool> is_secure_boot_enabled() {
    const auto sb = secure_boot();
    if (!sb) {
        return fail(sb.error());
    }
    return *sb == ::syscape::security::secure_boot_state::enabled;
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
    const auto content =
        openharmony::read_text_file("/proc/sys/kernel/randomize_va_space");
    if (!content) {
        return fail(content.error());
    }
    std::string_view val = *content;
    openharmony::strip_trailing_newlines(val);
    if (val == "2") {
        return ::syscape::security::aslr_mode::full;
    }
    if (val == "1") {
        return ::syscape::security::aslr_mode::partial;
    }
    if (val == "0") {
        return ::syscape::security::aslr_mode::disabled;
    }
    return fail(errc::malformed_data);
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
volume_encryption(std::string_view path) {
    if (path.empty()) {
        return fail(errc::invalid_argument);
    }
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
