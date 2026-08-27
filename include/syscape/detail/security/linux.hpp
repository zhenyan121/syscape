#ifndef SYSCAPE_DETAIL_SECURITY_LINUX_HPP
#define SYSCAPE_DETAIL_SECURITY_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

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

/// Queries the Address Space Layout Randomization (ASLR) mode from a sysctl root.
inline result<::syscape::security::aslr_mode> aslr_at(const char* path) {
    const auto res = lplat::read_text_file(path, 64U);
    if (!res) {
        if (res.error() == std::errc::no_such_file_or_directory) {
            return fail(errc::not_supported);
        }
        return fail(res.error());
    }
    return scomm::parse_aslr_mode(*res);
}

/// Queries the Address Space Layout Randomization (ASLR) mode on Linux.
inline result<::syscape::security::aslr_mode> aslr() {
    return aslr_at("/proc/sys/kernel/randomize_va_space");
}

/// Queries known CPU and platform hardware vulnerability mitigations from a sysfs directory.
inline result<std::vector<::syscape::security::cpu_vulnerability_entry>>
cpu_vulnerabilities_at(const char* dir_path) {
    std::vector<::syscape::security::cpu_vulnerability_entry> list;
    const lplat::directory_handle dir(dir_path);
    if (!dir.valid()) {
        if (dir.error() == ENOENT) {
            return fail(errc::not_supported);
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
        if (!is_valid_utf8(name)) {
            return fail(errc::invalid_encoding);
        }

        const std::string file_path =
            std::string(dir_path) + "/" + std::string(name);
        const auto content = lplat::read_text_file(file_path.c_str(), 2048U);
        if (!content) {
            if (content.error() == std::errc::no_such_file_or_directory) {
                continue;
            }
            return fail(content.error());
        }
        if (!is_valid_utf8(*content)) {
            return fail(errc::invalid_encoding);
        }

        const auto trimmed = scomm::trim_whitespace(*content);
        ::syscape::security::cpu_vulnerability_entry item;
        item.name = std::string(name);
        item.raw_description = std::string(trimmed);
        item.status = scomm::parse_vulnerability_status(trimmed);
        list.push_back(std::move(item));
    }

    std::sort(list.begin(), list.end(),
              [](const ::syscape::security::cpu_vulnerability_entry& a,
                 const ::syscape::security::cpu_vulnerability_entry& b) noexcept {
                  return a.name < b.name;
              });

    return list;
}

/// Queries known CPU and platform hardware vulnerability mitigations.
inline result<std::vector<::syscape::security::cpu_vulnerability_entry>>
cpu_vulnerabilities() {
    return cpu_vulnerabilities_at("/sys/devices/system/cpu/vulnerabilities");
}

/// Queries process capabilities from /proc/[pid]/status content using incremental POSIX read.
inline result<::syscape::security::process_capabilities>
capabilities_at(const char* status_path) {
    const int descriptor = ::open(status_path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        const int err = errno;
        if (err == ENOENT) {
            return fail(errc::not_supported);
        }
        if (err == EACCES) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(err, std::generic_category()));
    }
    const lplat::file_descriptor owned_fd(descriptor);
    static_cast<void>(owned_fd);

    std::uint64_t cap_inh_mask = 0U;
    std::uint64_t cap_prm_mask = 0U;
    std::uint64_t cap_eff_mask = 0U;
    std::uint64_t cap_bnd_mask = 0U;
    std::uint64_t cap_amb_mask = 0U;
    bool has_inh = false;
    bool has_prm = false;
    bool has_eff = false;
    bool has_bnd = false;
    bool has_amb = false;

    char buffer[4096];
    std::string current_line;
    bool eof_reached = false;

    while (!eof_reached) {
        const ssize_t count = ::read(descriptor, buffer, sizeof(buffer));
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (count == 0) {
            eof_reached = true;
        }

        std::size_t offset = 0U;
        const std::size_t bytes = static_cast<std::size_t>(count);

        while (offset < bytes || (eof_reached && !current_line.empty())) {
            std::size_t newline_pos = std::string::npos;
            if (offset < bytes) {
                const void* nl = std::memchr(buffer + offset, '\n', bytes - offset);
                if (nl != nullptr) {
                    newline_pos = static_cast<std::size_t>(
                        static_cast<const char*>(nl) - (buffer + offset));
                }
            }

            std::string_view line_view;
            if (newline_pos != std::string::npos) {
                if (current_line.empty()) {
                    line_view = std::string_view(buffer + offset, newline_pos);
                } else {
                    current_line.append(buffer + offset, newline_pos);
                    line_view = current_line;
                }
                offset += newline_pos + 1U;
            } else if (eof_reached) {
                line_view = current_line;
                current_line.clear();
            } else {
                current_line.append(buffer + offset, bytes - offset);
                offset = bytes;
                break;
            }

            // Process one complete line
            const auto colon = line_view.find(':');
            if (colon != std::string_view::npos) {
                const auto key = scomm::trim_whitespace(line_view.substr(0U, colon));
                if (key.rfind("Cap", 0) == 0) {
                    const auto val_str = scomm::trim_whitespace(line_view.substr(colon + 1U));
                    if (key == "CapInh") {
                        const auto parsed = scomm::parse_hex_u64(val_str);
                        if (!parsed) {
                            return fail(parsed.error());
                        }
                        cap_inh_mask = *parsed;
                        has_inh = true;
                    } else if (key == "CapPrm") {
                        const auto parsed = scomm::parse_hex_u64(val_str);
                        if (!parsed) {
                            return fail(parsed.error());
                        }
                        cap_prm_mask = *parsed;
                        has_prm = true;
                    } else if (key == "CapEff") {
                        const auto parsed = scomm::parse_hex_u64(val_str);
                        if (!parsed) {
                            return fail(parsed.error());
                        }
                        cap_eff_mask = *parsed;
                        has_eff = true;
                    } else if (key == "CapBnd") {
                        const auto parsed = scomm::parse_hex_u64(val_str);
                        if (!parsed) {
                            return fail(parsed.error());
                        }
                        cap_bnd_mask = *parsed;
                        has_bnd = true;
                    } else if (key == "CapAmb") {
                        const auto parsed = scomm::parse_hex_u64(val_str);
                        if (!parsed) {
                            return fail(parsed.error());
                        }
                        cap_amb_mask = *parsed;
                        has_amb = true;
                    }
                }
            }

            current_line.clear();
            if (eof_reached && offset >= bytes) {
                break;
            }
        }
    }

    if (!has_inh && !has_prm && !has_eff) {
        return fail(errc::malformed_data);
    }

    ::syscape::security::process_capabilities caps;
    if (has_eff) {
        caps.effective = scomm::decode_linux_capabilities(cap_eff_mask);
    }
    if (has_prm) {
        caps.permitted = scomm::decode_linux_capabilities(cap_prm_mask);
    }
    if (has_inh) {
        caps.inheritable = scomm::decode_linux_capabilities(cap_inh_mask);
    }
    if (has_bnd) {
        caps.bounding = scomm::decode_linux_capabilities(cap_bnd_mask);
    }
    if (has_amb) {
        caps.ambient = scomm::decode_linux_capabilities(cap_amb_mask);
    }

    return caps;
}

/// Queries the fine-grained process capabilities of the calling process.
inline result<::syscape::security::process_capabilities> capabilities() {
    return capabilities_at("/proc/self/status");
}

/// Queries the process privileges of the calling process on Linux.
inline result<std::vector<::syscape::security::privilege_entry>> privileges() {
    const auto caps_res = capabilities();
    if (!caps_res) {
        return fail(caps_res.error());
    }

    std::vector<::syscape::security::privilege_entry> privs;
    for (const auto& cap : caps_res->effective) {
        ::syscape::security::privilege_entry entry;
        entry.name = cap;
        entry.enabled = true;
        entry.enabled_by_default = false;
        privs.push_back(std::move(entry));
    }
    for (const auto& cap : caps_res->permitted) {
        bool already = false;
        for (const auto& existing : privs) {
            if (existing.name == cap) {
                already = true;
                break;
            }
        }
        if (!already) {
            ::syscape::security::privilege_entry entry;
            entry.name = cap;
            entry.enabled = false;
            entry.enabled_by_default = false;
            privs.push_back(std::move(entry));
        }
    }

    std::sort(privs.begin(), privs.end(),
              [](const ::syscape::security::privilege_entry& a,
                 const ::syscape::security::privilege_entry& b) noexcept {
                  return a.name < b.name;
              });

    return privs;
}

/// Recursively inspects a block device (e.g. dm-0, sda1) and its slaves in /sys/block/*/slaves
/// to detect underlying encryption, properly reporting mixed/unknown states for multi-slave layouts.
inline std::pair<::syscape::security::encryption_state, ::syscape::security::encryption_type>
inspect_block_device_encryption(const std::string& block_name,
                                const char* sys_block_root,
                                int depth = 0) {
    if (depth > 8 || block_name.empty()) {
        return {::syscape::security::encryption_state::unknown,
                ::syscape::security::encryption_type::unknown};
    }

    const std::string dev_dir = std::string(sys_block_root) + "/" + block_name;
    const std::string uuid_file = dev_dir + "/dm/uuid";
    const auto uuid_res = lplat::read_text_file(uuid_file.c_str(), 512U);
    if (uuid_res) {
        const auto enc = scomm::parse_dm_uuid_encryption(*uuid_res);
        if (enc.first == ::syscape::security::encryption_state::encrypted) {
            return enc;
        }
    } else if (uuid_res.error() != std::errc::no_such_file_or_directory) {
        return {::syscape::security::encryption_state::unknown,
                ::syscape::security::encryption_type::unknown};
    }

    // Inspect slaves directory (e.g. LVM logical volume on top of multiple physical volumes)
    const std::string slaves_dir = dev_dir + "/slaves";
    const lplat::directory_handle slaves(slaves_dir.c_str());
    if (slaves.valid()) {
        std::size_t total_slaves = 0U;
        std::size_t encrypted_slaves = 0U;
        std::size_t unencrypted_slaves = 0U;
        ::syscape::security::encryption_type common_type = ::syscape::security::encryption_type::none;

        for (;;) {
            errno = 0;
            const auto* entry = ::readdir(slaves.get());
            if (entry == nullptr) {
                if (errno != 0) {
                    return {::syscape::security::encryption_state::unknown,
                            ::syscape::security::encryption_type::unknown};
                }
                break;
            }
            const std::string_view sname(entry->d_name);
            if (sname == "." || sname == "..") {
                continue;
            }
            ++total_slaves;
            const auto sub_enc = inspect_block_device_encryption(
                std::string(sname), sys_block_root, depth + 1);

            if (sub_enc.first == ::syscape::security::encryption_state::encrypted) {
                ++encrypted_slaves;
                if (common_type == ::syscape::security::encryption_type::none) {
                    common_type = sub_enc.second;
                } else if (common_type != sub_enc.second) {
                    common_type = ::syscape::security::encryption_type::other;
                }
            } else if (sub_enc.first == ::syscape::security::encryption_state::unencrypted) {
                ++unencrypted_slaves;
            }
        }

        if (total_slaves > 0U) {
            if (encrypted_slaves == total_slaves) {
                return {::syscape::security::encryption_state::encrypted, common_type};
            }
            if (unencrypted_slaves == total_slaves) {
                return {::syscape::security::encryption_state::unencrypted, ::syscape::security::encryption_type::none};
            }
            if (encrypted_slaves > 0U && unencrypted_slaves > 0U &&
                encrypted_slaves + unencrypted_slaves == total_slaves) {
                return {::syscape::security::encryption_state::mixed, common_type};
            }
            return {::syscape::security::encryption_state::unknown, ::syscape::security::encryption_type::unknown};
        }
    } else if (slaves.error() != ENOENT) {
        return {::syscape::security::encryption_state::unknown,
                ::syscape::security::encryption_type::unknown};
    }

    // A block device without crypt layer cannot be proven unencrypted because filesystem-level
    // or hardware encryption (e.g. fscrypt, native ZFS, OPAL SED) cannot be ruled out.
    return {::syscape::security::encryption_state::unknown,
            ::syscape::security::encryption_type::unknown};
}

inline result<::syscape::security::volume_encryption_info>
volume_encryption_at(std::string_view path,
                     const char* sys_block_root,
                     const char* mounts_path) {
    if (path.empty()) {
        return fail(errc::invalid_argument);
    }
    if (!is_valid_utf8(path)) {
        return fail(errc::invalid_encoding);
    }
    if (path.find('\0') != std::string_view::npos) {
        return fail(errc::invalid_argument);
    }

    const std::string path_str(path);
    struct ::stat st {};
    if (::stat(path_str.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    ::syscape::security::volume_encryption_info info;
    info.path_or_device = path_str;
    info.state = ::syscape::security::encryption_state::unknown;
    info.type = ::syscape::security::encryption_type::unknown;

    char resolved[4096];
    std::string target_path = path_str;
    if (::realpath(path_str.c_str(), resolved) != nullptr) {
        target_path = resolved;
    }

    const auto mounts = lplat::read_text_file(mounts_path, 65536U);
    if (!mounts) {
        if (mounts.error() == std::errc::no_such_file_or_directory) {
            return fail(errc::not_supported);
        }
        return fail(mounts.error());
    }

    // Find the longest matching mount point
    std::string matched_device;
    std::size_t longest_mount_len = 0U;

    std::string_view text(*mounts);
    while (!text.empty()) {
        const auto newline = text.find('\n');
        const auto line = text.substr(0U, newline);
        if (!line.empty() && line[0] != '#') {
            const auto space1 = line.find(' ');
            if (space1 != std::string_view::npos) {
                const auto raw_dev = line.substr(0U, space1);
                const auto rest = line.substr(space1 + 1U);
                const auto space2 = rest.find(' ');
                if (space2 != std::string_view::npos) {
                    const auto raw_mnt = rest.substr(0U, space2);
                    const auto mnt = scomm::decode_mount_entry(raw_mnt);
                    const auto dev = scomm::decode_mount_entry(raw_dev);
                    if (target_path == mnt ||
                        (target_path.size() > mnt.size() &&
                         target_path.substr(0U, mnt.size()) == mnt &&
                         (mnt == "/" || target_path[mnt.size()] == '/'))) {
                        if (mnt.size() >= longest_mount_len) {
                            longest_mount_len = mnt.size();
                            matched_device = dev;
                        }
                    }
                }
            }
        }
        if (newline == std::string_view::npos) {
            break;
        }
        text.remove_prefix(newline + 1U);
    }

    // If matched_device is empty, check if target_path itself is a dm/device path
    if (matched_device.empty()) {
        if (target_path.rfind("/dev/dm-", 0) == 0 ||
            target_path.rfind("/dev/mapper/", 0) == 0 ||
            target_path.rfind("dm-", 0) == 0 ||
            target_path.rfind("/dev/", 0) == 0) {
            matched_device = target_path;
        }
    }

    if (matched_device.empty()) {
        return info;
    }

    // Determine the sysfs block node name (e.g. dm-0, sda1, nvme0n1p2)
    std::string block_node;
    if (matched_device.rfind("/dev/dm-", 0) == 0) {
        block_node = matched_device.substr(5U); // dm-0
    } else if (matched_device.rfind("dm-", 0) == 0) {
        block_node = matched_device;
    } else if (matched_device.rfind("/dev/mapper/", 0) == 0) {
        const std::string mapper_name = matched_device.substr(12U);
        // Find corresponding dm-* in sys_block_root
        const lplat::directory_handle block_dir(sys_block_root);
        if (block_dir.valid()) {
            for (;;) {
                errno = 0;
                const auto* entry = ::readdir(block_dir.get());
                if (entry == nullptr) {
                    break;
                }
                const std::string_view bname(entry->d_name);
                if (bname.rfind("dm-", 0) == 0) {
                    const std::string name_file =
                        std::string(sys_block_root) + "/" + std::string(bname) + "/dm/name";
                    const auto name_res = lplat::read_text_file(name_file.c_str(), 256U);
                    if (name_res && scomm::trim_whitespace(*name_res) == mapper_name) {
                        block_node = std::string(bname);
                        break;
                    }
                }
            }
        }
    } else if (matched_device.rfind("/dev/", 0) == 0) {
        block_node = matched_device.substr(5U); // e.g. sda1
    }

    if (!block_node.empty()) {
        const auto enc = inspect_block_device_encryption(block_node, sys_block_root);
        info.state = enc.first;
        info.type = enc.second;
    }

    return info;
}

/// Queries the encryption status of the volume containing the specified path.
inline result<::syscape::security::volume_encryption_info>
volume_encryption(std::string_view path) {
    return volume_encryption_at(path, "/sys/block", "/proc/self/mounts");
}

/// Queries all observable encrypted storage volumes on Linux.
inline result<std::vector<::syscape::security::volume_encryption_info>>
encrypted_volumes_at(const char* sys_block_root, const char* mounts_path) {
    std::vector<::syscape::security::volume_encryption_info> list;
    const lplat::directory_handle block_dir(sys_block_root);
    if (!block_dir.valid()) {
        if (block_dir.error() == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(block_dir.error(), std::generic_category()));
    }

    // Read mounts map to correlate dm names/nodes with mount points
    const auto mounts = lplat::read_text_file(mounts_path, 65536U);

    for (;;) {
        errno = 0;
        const auto* entry = ::readdir(block_dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }

        const std::string_view name(entry->d_name);
        if (name.rfind("dm-", 0) != 0) {
            continue;
        }

        const auto enc = inspect_block_device_encryption(std::string(name), sys_block_root);
        if (enc.first != ::syscape::security::encryption_state::encrypted) {
            continue;
        }

        const std::string dm_dir =
            std::string(sys_block_root) + "/" + std::string(name);
        std::string dm_name;
        const std::string name_file = dm_dir + "/dm/name";
        const auto name_res = lplat::read_text_file(name_file.c_str(), 256U);
        if (name_res) {
            dm_name = std::string(scomm::trim_whitespace(*name_res));
        }

        ::syscape::security::volume_encryption_info info;
        info.path_or_device = dm_name.empty() ?
            ("/dev/" + std::string(name)) :
            ("/dev/mapper/" + dm_name);
        info.state = enc.first;
        info.type = enc.second;

        // Try to locate mount point
        if (mounts) {
            std::string_view text(*mounts);
            while (!text.empty()) {
                const auto newline = text.find('\n');
                const auto line = text.substr(0U, newline);
                if (!line.empty() && line[0] != '#') {
                    const auto space1 = line.find(' ');
                    if (space1 != std::string_view::npos) {
                        const auto raw_dev = line.substr(0U, space1);
                        const auto dev = scomm::decode_mount_entry(raw_dev);
                        if (dev == info.path_or_device ||
                            dev == ("/dev/" + std::string(name))) {
                            const auto rest = line.substr(space1 + 1U);
                            const auto space2 = rest.find(' ');
                            if (space2 != std::string_view::npos) {
                                const auto raw_mnt = rest.substr(0U, space2);
                                info.path_or_device = scomm::decode_mount_entry(raw_mnt);
                                break;
                            }
                        }
                    }
                }
                if (newline == std::string_view::npos) {
                    break;
                }
                text.remove_prefix(newline + 1U);
            }
        }

        list.push_back(std::move(info));
    }

    std::sort(list.begin(), list.end(),
              [](const ::syscape::security::volume_encryption_info& a,
                 const ::syscape::security::volume_encryption_info& b) noexcept {
                  return a.path_or_device < b.path_or_device;
              });

    return list;
}

/// Queries all observable encrypted storage volumes on the system.
inline result<std::vector<::syscape::security::volume_encryption_info>>
encrypted_volumes() {
    return encrypted_volumes_at("/sys/block", "/proc/self/mounts");
}

} // namespace security_backend
} // namespace detail
} // namespace syscape

#endif
