#ifndef SYSCAPE_DETAIL_SECURITY_FREEBSD_HPP
#define SYSCAPE_DETAIL_SECURITY_FREEBSD_HPP

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
    std::vector<std::string> modules;
    std::size_t size = 0U;
    if (::sysctlbyname("security.mac.labels", nullptr, &size, nullptr, 0U) ==
            0 &&
        size > 0U) {
        std::string labels(size, '\0');
        if (::sysctlbyname("security.mac.labels", &labels[0], &size, nullptr,
                           0U) == 0) {
            while (!labels.empty() &&
                   (labels.back() == '\0' || labels.back() == '\n')) {
                labels.pop_back();
            }
            if (!labels.empty()) {
                std::size_t start = 0U;
                while (start < labels.size()) {
                    std::size_t comma = labels.find(',', start);
                    std::string token =
                        labels.substr(start, comma == std::string::npos
                                                 ? labels.size() - start
                                                 : comma - start);
                    while (!token.empty() && token.front() == ' ') {
                        token.erase(token.begin());
                    }
                    while (!token.empty() && token.back() == ' ') {
                        token.pop_back();
                    }
                    if (!token.empty()) {
                        modules.push_back(std::move(token));
                    }
                    if (comma == std::string::npos) {
                        break;
                    }
                    start = comma + 1U;
                }
            }
        }
    }
    if (modules.empty()) {
        return fail(errc::not_supported);
    }
    return modules;
}

inline result<::syscape::security::lockdown_mode> lockdown() {
    return fail(errc::not_supported);
}

inline result<bool> is_sip_enabled() {
    return fail(errc::not_supported);
}

inline result<::syscape::security::aslr_mode> aslr() {
    const result<int> aslr_val = read_sysctl_int("vm.aslr_enable");
    if (!aslr_val) {
        return fail(aslr_val.error());
    }
    if (*aslr_val == 0) {
        return ::syscape::security::aslr_mode::disabled;
    }
    if (*aslr_val == 1) {
        return ::syscape::security::aslr_mode::partial;
    }
    return ::syscape::security::aslr_mode::full;
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
