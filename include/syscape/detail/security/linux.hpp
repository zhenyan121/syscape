#ifndef SYSCAPE_DETAIL_SECURITY_LINUX_HPP
#define SYSCAPE_DETAIL_SECURITY_LINUX_HPP

#include <cerrno>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <sys/stat.h>

#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/security/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace security_backend {

namespace lplat = ::syscape::detail::linux_platform;
namespace scomm = ::syscape::detail::security_common;

inline result<bool> read_efi_boolean_variable(
    const std::string& efi_root, const char* variable_name) {
    constexpr const char* global_variable_guid =
        "8be4df61-93ca-11d2-aa0d-00e098032b8c";
    const std::string suffix =
        std::string(variable_name) + "-" + global_variable_guid;
    const std::string efivar_path = efi_root + "/efivars/" + suffix;
    const auto efivar = lplat::read_text_file(efivar_path.c_str(), 64U);
    if (efivar) {
        const auto parsed = scomm::parse_efivar_secure_boot_payload(
            efivar->data(), efivar->size());
        if (!parsed) {
            return fail(parsed.error());
        }
        return *parsed == ::syscape::security::secure_boot_state::enabled;
    }
    if (efivar.error() != std::errc::no_such_file_or_directory) {
        return fail(efivar.error());
    }

    const std::string legacy_path =
        efi_root + "/vars/" + suffix + "/data";
    const auto legacy = lplat::read_text_file(legacy_path.c_str(), 64U);
    if (legacy) {
        const auto parsed = scomm::parse_efivar_secure_boot_payload(
            legacy->data(), legacy->size());
        if (!parsed) {
            return fail(parsed.error());
        }
        return *parsed == ::syscape::security::secure_boot_state::enabled;
    }
    if (legacy.error() == std::errc::no_such_file_or_directory) {
        return fail(errc::not_found);
    }
    return fail(legacy.error());
}

/// Queries the UEFI Secure Boot enablement state below an EFI sysfs root.
inline result<::syscape::security::secure_boot_state> secure_boot_at(
    const std::string& efi_root) {
    // Check if system booted via UEFI
    struct ::stat st {};
    if (::stat(efi_root.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    const auto secure = read_efi_boolean_variable(efi_root, "SecureBoot");
    if (!secure) {
        if (secure.error() == errc::not_found) {
            return fail(errc::not_supported);
        }
        return fail(secure.error());
    }
    if (*secure) {
        return ::syscape::security::secure_boot_state::enabled;
    }

    const auto audit = read_efi_boolean_variable(efi_root, "AuditMode");
    if (audit) {
        if (*audit) {
            return ::syscape::security::secure_boot_state::audit;
        }
    } else if (audit.error() != errc::not_found) {
        return fail(audit.error());
    }

    const auto setup = read_efi_boolean_variable(efi_root, "SetupMode");
    if (setup) {
        if (*setup) {
            return ::syscape::security::secure_boot_state::audit;
        }
    } else if (setup.error() != errc::not_found) {
        return fail(setup.error());
    }

    return ::syscape::security::secure_boot_state::disabled;
}

/// Queries the UEFI Secure Boot enablement state on Linux.
inline result<::syscape::security::secure_boot_state> secure_boot() {
    return secure_boot_at("/sys/firmware/efi");
}

/// Queries whether UEFI Secure Boot is active and enforcing.
inline result<bool> is_secure_boot_enabled() {
    const auto res = secure_boot();
    if (!res) {
        return fail(res.error());
    }
    return *res == ::syscape::security::secure_boot_state::enabled;
}

inline bool is_tpm_device_name(std::string_view name) noexcept {
    if (name.size() <= 3U || name.substr(0U, 3U) != "tpm") {
        return false;
    }
    for (std::size_t index = 3U; index < name.size(); ++index) {
        if (name[index] < '0' || name[index] > '9') {
            return false;
        }
    }
    return true;
}

inline result<std::optional<std::string>> tpm_driver_description(
    const std::string& device_path) {
    const std::string paths[] = {
        device_path + "/device/uevent",
        device_path + "/uevent"
    };
    for (const auto& path : paths) {
        const auto uevent = lplat::read_text_file(path.c_str(), 4096U);
        if (!uevent) {
            if (uevent.error() == std::errc::no_such_file_or_directory) {
                continue;
            }
            return fail(uevent.error());
        }
        const auto driver = scomm::find_uevent_value(*uevent, "DRIVER");
        if (driver.empty()) {
            continue;
        }
        if (!is_valid_utf8(driver)) {
            return fail(errc::invalid_encoding);
        }
        return std::optional<std::string>(std::string(driver));
    }
    return std::optional<std::string>();
}

/// Queries TPM presence and version information below a TPM class root.
inline result<::syscape::security::tpm_info> tpm_at(const char* class_root) {
    ::syscape::security::tpm_info info;
    info.present = false;
    info.version = ::syscape::security::tpm_version::none;

    const lplat::directory_handle dir(class_root);
    if (!dir.valid()) {
        if (dir.error() == ENOENT) {
            return info;
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    for (;;) {
        errno = 0;
        const auto* entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }

        const std::string_view name(entry->d_name);
        if (name == "." || name == "..") {
            continue;
        }

        if (is_tpm_device_name(name)) {
            info.present = true;
            info.device_id = std::string(name);

            const std::string device_path =
                std::string(class_root) + "/" + std::string(name);
            const std::string ver_path = device_path + "/tpm_version_major";
            const auto ver_res = lplat::read_text_file(ver_path.c_str(), 64U);
            if (ver_res) {
                if (!is_valid_utf8(*ver_res)) {
                    return fail(errc::invalid_encoding);
                }
                auto parsed = scomm::parse_tpm_version_string(*ver_res);
                info.version = parsed.first;
                info.version_string = std::move(parsed.second);
            } else if (ver_res.error() == std::errc::no_such_file_or_directory) {
                info.version = ::syscape::security::tpm_version::unknown;
            } else {
                return fail(ver_res.error());
            }

            const auto description = tpm_driver_description(device_path);
            if (!description) {
                return fail(description.error());
            }
            info.description = *description;
            return info;
        }
    }

    return info;
}

/// Queries Trusted Platform Module (TPM) presence and version information.
inline result<::syscape::security::tpm_info> tpm() {
    return tpm_at("/sys/class/tpm");
}

inline result<std::vector<std::string>> security_modules_at(const char* path) {
    const auto lsm_res = lplat::read_text_file(path, 1024U);
    if (lsm_res) {
        auto modules = scomm::parse_lsm_string(*lsm_res);
        if (modules.empty()) {
            return fail(errc::malformed_data);
        }
        for (const auto& module : modules) {
            if (!is_valid_utf8(module)) {
                return fail(errc::invalid_encoding);
            }
        }
        return modules;
    }
    if (lsm_res.error() == std::errc::no_such_file_or_directory) {
        return fail(errc::not_supported);
    }
    return fail(lsm_res.error());
}

/// Queries the list of active kernel security modules (LSMs).
inline result<std::vector<std::string>> security_modules() {
    return security_modules_at("/sys/kernel/security/lsm");
}

inline result<::syscape::security::lockdown_mode> lockdown_at(const char* path) {
    const auto res = lplat::read_text_file(path, 256U);
    if (!res) {
        if (res.error() == std::errc::no_such_file_or_directory) {
            return fail(errc::not_supported);
        }
        return fail(res.error());
    }
    return scomm::parse_lockdown_line(*res);
}

/// Queries the Linux kernel lockdown protection level.
inline result<::syscape::security::lockdown_mode> lockdown() {
    return lockdown_at("/sys/kernel/security/lockdown");
}

/// macOS System Integrity Protection is not supported on Linux.
inline result<bool> is_sip_enabled() {
    return fail(errc::not_supported);
}

} // namespace security_backend
} // namespace detail
} // namespace syscape

#endif
