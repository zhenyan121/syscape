#ifndef SYSCAPE_DETAIL_SECURITY_COMMON_HPP
#define SYSCAPE_DETAIL_SECURITY_COMMON_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace security_common {

using secure_boot_type = ::syscape::security::secure_boot_state;
using tpm_version_type = ::syscape::security::tpm_version;
using lockdown_type = ::syscape::security::lockdown_mode;
using aslr_type = ::syscape::security::aslr_mode;
using mitigation_type = ::syscape::security::mitigation_status;
using encryption_state_type = ::syscape::security::encryption_state;
using encryption_type_type = ::syscape::security::encryption_type;

/// Trims leading and trailing ASCII whitespace.
inline std::string_view trim_whitespace(std::string_view text) noexcept {
    std::size_t start = 0U;
    while (start < text.size() &&
           (text[start] == ' ' || text[start] == '\t' || text[start] == '\r' ||
            text[start] == '\n')) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start &&
           (text[end - 1U] == ' ' || text[end - 1U] == '\t' ||
            text[end - 1U] == '\r' || text[end - 1U] == '\n')) {
        --end;
    }
    return text.substr(start, end - start);
}

/// Parses an efivar binary buffer for SecureBoot variable state.
///
/// Standard Linux sysfs efivars (/sys/firmware/efi/efivars/) store 4 bytes of
/// EFI variable attributes followed by data bytes. For SecureBoot, the data byte
/// is 0 (disabled) or 1 (enabled). Legacy sysfs efivars (/sys/firmware/efi/vars/.../data)
/// store only the data byte without attributes.
inline result<secure_boot_type> parse_efivar_secure_boot_payload(
    const void* data, std::size_t size) {
    if (data == nullptr || size == 0U) {
        return fail(errc::malformed_data);
    }
    const auto* bytes = static_cast<const unsigned char*>(data);

    unsigned char val = 0U;
    if (size == 5U) {
        // 4 bytes attributes + 1 byte data
        val = bytes[4];
    } else if (size == 1U) {
        // Legacy 1-byte raw data
        val = bytes[0];
    } else {
        return fail(errc::malformed_data);
    }

    if (val == 1U) {
        return secure_boot_type::enabled;
    }
    if (val == 0U) {
        return secure_boot_type::disabled;
    }
    return fail(errc::malformed_data);
}

/// Parses a Linux /sys/kernel/security/lockdown line containing bracketed mode,
/// e.g. "[none] integrity confidentiality", "none [integrity] confidentiality",
/// or "none integrity [confidentiality]".
inline result<lockdown_type> parse_lockdown_line(std::string_view text) {
    const auto open_pos = text.find('[');
    if (open_pos == std::string_view::npos) {
        return fail(errc::malformed_data);
    }
    const auto close_pos = text.find(']', open_pos + 1U);
    if (close_pos == std::string_view::npos) {
        return fail(errc::malformed_data);
    }

    const auto mode_str =
        trim_whitespace(text.substr(open_pos + 1U, close_pos - open_pos - 1U));
    if (mode_str.empty()) {
        return fail(errc::malformed_data);
    }
    if (mode_str == "none") {
        return lockdown_type::none;
    }
    if (mode_str == "integrity") {
        return lockdown_type::integrity;
    }
    if (mode_str == "confidentiality") {
        return lockdown_type::confidentiality;
    }
    return lockdown_type::unknown;
}

/// Returns a value from a line-oriented uevent payload.
inline std::string_view find_uevent_value(
    std::string_view text, std::string_view key) noexcept {
    while (!text.empty()) {
        const auto newline = text.find('\n');
        const auto line = text.substr(0U, newline);
        if (line.size() > key.size() &&
            line.compare(0U, key.size(), key) == 0 &&
            line[key.size()] == '=') {
            return trim_whitespace(line.substr(key.size() + 1U));
        }
        if (newline == std::string_view::npos) {
            break;
        }
        text.remove_prefix(newline + 1U);
    }
    return {};
}

/// Parses a comma-separated (or whitespace-separated) list of Linux Security Modules (LSMs).
inline std::vector<std::string> parse_lsm_string(std::string_view text) {
    std::vector<std::string> result;
    std::size_t start = 0U;
    while (start < text.size()) {
        const auto sep = text.find_first_of(", \t\r\n", start);
        const auto token_len =
            (sep == std::string_view::npos) ? text.size() - start : sep - start;
        const auto token = trim_whitespace(text.substr(start, token_len));
        if (!token.empty()) {
            std::string s(token);
            // Deduplicate tokens while preserving order
            bool exists = false;
            for (const auto& item : result) {
                if (item == s) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                result.push_back(std::move(s));
            }
        }
        if (sep == std::string_view::npos) {
            break;
        }
        start = sep + 1U;
    }
    return result;
}

/// Classifies a TPM version string from sysfs tpm_version_major or similar source.
inline std::pair<tpm_version_type, std::string> parse_tpm_version_string(
    std::string_view text) {
    const auto trimmed = trim_whitespace(text);
    if (trimmed.empty()) {
        return {tpm_version_type::unknown, ""};
    }
    if (trimmed == "2" || trimmed == "2.0") {
        return {tpm_version_type::v2_0, "2.0"};
    }
    if (trimmed == "1" || trimmed == "1.2") {
        return {tpm_version_type::v1_2, "1.2"};
    }
    return {tpm_version_type::other, std::string(trimmed)};
}

/// Parses an ASLR randomize_va_space integer string.
inline result<aslr_type> parse_aslr_mode(std::string_view text) {
    const auto trimmed = trim_whitespace(text);
    if (trimmed.empty()) {
        return fail(errc::malformed_data);
    }
    if (trimmed == "0") {
        return aslr_type::disabled;
    }
    if (trimmed == "1") {
        return aslr_type::partial;
    }
    if (trimmed == "2") {
        return aslr_type::full;
    }
    for (const char c : trimmed) {
        if (c < '0' || c > '9') {
            return fail(errc::malformed_data);
        }
    }
    return aslr_type::unknown;
}

/// Classifies CPU hardware vulnerability mitigation status from kernel sysfs text.
inline mitigation_type parse_vulnerability_status(std::string_view text) noexcept {
    const auto trimmed = trim_whitespace(text);
    if (trimmed.empty()) {
        return mitigation_type::unknown;
    }

    auto starts_with_ci = [](std::string_view str, std::string_view prefix) noexcept {
        if (str.size() < prefix.size()) {
            return false;
        }
        for (std::size_t i = 0; i < prefix.size(); ++i) {
            char a = str[i];
            char b = prefix[i];
            if (a >= 'A' && a <= 'Z') {
                a = static_cast<char>(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = static_cast<char>(b - 'A' + 'a');
            }
            if (a != b) {
                return false;
            }
        }
        return true;
    };

    if (starts_with_ci(trimmed, "not affected") ||
        starts_with_ci(trimmed, "kvm: not affected")) {
        return mitigation_type::not_affected;
    }
    if (starts_with_ci(trimmed, "mitigation: disabled") ||
        starts_with_ci(trimmed, "disabled")) {
        return mitigation_type::disabled;
    }
    if (starts_with_ci(trimmed, "mitigation:") ||
        starts_with_ci(trimmed, "mitigated") ||
        starts_with_ci(trimmed, "kvm: mitigation:") ||
        starts_with_ci(trimmed, "kvm: mitigated")) {
        return mitigation_type::mitigated;
    }
    if (starts_with_ci(trimmed, "vulnerable") ||
        starts_with_ci(trimmed, "kvm: vulnerable")) {
        return mitigation_type::vulnerable;
    }

    auto contains_ci = [&starts_with_ci](std::string_view str, std::string_view sub) noexcept {
        if (str.size() < sub.size()) {
            return false;
        }
        for (std::size_t i = 0; i <= str.size() - sub.size(); ++i) {
            if (starts_with_ci(str.substr(i), sub)) {
                return true;
            }
        }
        return false;
    };

    if (contains_ci(trimmed, "not affected")) {
        return mitigation_type::not_affected;
    }
    if (contains_ci(trimmed, "mitigation: disabled") || contains_ci(trimmed, "disabled")) {
        return mitigation_type::disabled;
    }
    if (contains_ci(trimmed, "mitigation:") || contains_ci(trimmed, "mitigated")) {
        return mitigation_type::mitigated;
    }
    if (contains_ci(trimmed, "vulnerable")) {
        return mitigation_type::vulnerable;
    }
    return mitigation_type::unknown;
}

/// Parses a hexadecimal 64-bit integer string.
inline result<std::uint64_t> parse_hex_u64(std::string_view text) {
    const auto trimmed = trim_whitespace(text);
    if (trimmed.empty()) {
        return fail(errc::malformed_data);
    }
    std::size_t start = 0U;
    if (trimmed.size() >= 2U && trimmed[0] == '0' &&
        (trimmed[1] == 'x' || trimmed[1] == 'X')) {
        start = 2U;
    }
    if (start >= trimmed.size()) {
        return fail(errc::malformed_data);
    }
    std::uint64_t val = 0U;
    for (std::size_t i = start; i < trimmed.size(); ++i) {
        const char c = trimmed[i];
        unsigned digit = 0U;
        if (c >= '0' && c <= '9') {
            digit = static_cast<unsigned>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = static_cast<unsigned>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<unsigned>(c - 'A' + 10);
        } else {
            return fail(errc::malformed_data);
        }
        if (val > (0xFFFFFFFFFFFFFFFFULL >> 4U)) {
            return fail(errc::value_too_large);
        }
        val = (val << 4U) | digit;
    }
    return val;
}

/// Decodes a 64-bit Linux capability bitmask into standard capability names.
inline std::vector<std::string> decode_linux_capabilities(std::uint64_t mask) {
    static constexpr const char* const k_cap_names[] = {
        "cap_chown",              // 0
        "cap_dac_override",        // 1
        "cap_dac_read_search",     // 2
        "cap_fowner",              // 3
        "cap_fsetid",              // 4
        "cap_kill",                // 5
        "cap_setgid",              // 6
        "cap_setuid",              // 7
        "cap_setpcap",             // 8
        "cap_linux_immutable",     // 9
        "cap_net_bind_service",    // 10
        "cap_net_broadcast",       // 11
        "cap_net_admin",           // 12
        "cap_net_raw",             // 13
        "cap_ipc_lock",            // 14
        "cap_ipc_owner",           // 15
        "cap_sys_module",          // 16
        "cap_sys_rawio",           // 17
        "cap_sys_chroot",          // 18
        "cap_sys_ptrace",          // 19
        "cap_sys_pacct",           // 20
        "cap_sys_admin",           // 21
        "cap_sys_boot",            // 22
        "cap_sys_nice",            // 23
        "cap_sys_resource",        // 24
        "cap_sys_time",            // 25
        "cap_sys_tty_config",      // 26
        "cap_mknod",               // 27
        "cap_lease",               // 28
        "cap_audit_write",         // 29
        "cap_audit_control",       // 30
        "cap_setfcap",             // 31
        "cap_mac_override",        // 32
        "cap_mac_admin",           // 33
        "cap_syslog",              // 34
        "cap_wake_alarm",          // 35
        "cap_block_suspend",       // 36
        "cap_audit_read",          // 37
        "cap_perfmon",             // 38
        "cap_bpf",                 // 39
        "cap_checkpoint_restore"   // 40
    };
    constexpr std::size_t k_known_caps = sizeof(k_cap_names) / sizeof(k_cap_names[0]);

    std::vector<std::string> caps;
    for (std::size_t bit = 0; bit < 64U; ++bit) {
        if ((mask & (static_cast<std::uint64_t>(1U) << bit)) != 0U) {
            if (bit < k_known_caps) {
                caps.emplace_back(k_cap_names[bit]);
            } else {
                caps.emplace_back("cap_" + std::to_string(bit));
            }
        }
    }
    return caps;
}

/// Parses device-mapper UUID to classify encryption technology.
inline std::pair<encryption_state_type, encryption_type_type>
parse_dm_uuid_encryption(std::string_view uuid) noexcept {
    const auto trimmed = trim_whitespace(uuid);
    if (trimmed.rfind("CRYPT-LUKS2-", 0) == 0 ||
        trimmed.rfind("CRYPT-LUKS1-", 0) == 0 ||
        trimmed.rfind("CRYPT-LUKS-", 0) == 0) {
        return {encryption_state_type::encrypted, encryption_type_type::luks};
    }
    if (trimmed.rfind("CRYPT-PLAIN-", 0) == 0) {
        return {encryption_state_type::encrypted, encryption_type_type::dm_crypt};
    }
    if (trimmed.rfind("CRYPT-", 0) == 0) {
        return {encryption_state_type::encrypted, encryption_type_type::other};
    }
    return {encryption_state_type::unknown, encryption_type_type::unknown};
}

/// Decodes octal escape sequences commonly found in /proc/mounts (e.g. \040 for space, \011 for tab).
inline std::string decode_mount_entry(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 3 < text.size() &&
            text[i + 1] >= '0' && text[i + 1] <= '7' &&
            text[i + 2] >= '0' && text[i + 2] <= '7' &&
            text[i + 3] >= '0' && text[i + 3] <= '7') {
            const unsigned char val = static_cast<unsigned char>(
                ((text[i + 1] - '0') << 6) |
                ((text[i + 2] - '0') << 3) |
                (text[i + 3] - '0'));
            result.push_back(static_cast<char>(val));
            i += 3;
        } else {
            result.push_back(text[i]);
        }
    }
    return result;
}

} // namespace security_common
} // namespace detail
} // namespace syscape

#endif
