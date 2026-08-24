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

} // namespace security_common
} // namespace detail
} // namespace syscape

#endif
