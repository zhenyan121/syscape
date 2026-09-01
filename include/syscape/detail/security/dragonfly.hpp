#ifndef SYSCAPE_DETAIL_SECURITY_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_SECURITY_DRAGONFLY_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/detail/security/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace security_backend {

inline result<int> read_sysctl_int(const char* name) {
    int value = 0;
    std::size_t size = sizeof(value);
    if (::sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    return value;
}

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
    const result<int> aslr_val = read_sysctl_int("vm.aslr_enable");
    if (aslr_val) {
        if (*aslr_val == 0) {
            return ::syscape::security::aslr_mode::disabled;
        }
        if (*aslr_val == 1) {
            return ::syscape::security::aslr_mode::partial;
        }
        if (*aslr_val >= 2) {
            return ::syscape::security::aslr_mode::full;
        }
    }
    const result<int> mmap_val = read_sysctl_int("vm.randomize_mmap");
    if (mmap_val) {
        if (*mmap_val == 0) {
            return ::syscape::security::aslr_mode::disabled;
        }
        return ::syscape::security::aslr_mode::full;
    }
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
