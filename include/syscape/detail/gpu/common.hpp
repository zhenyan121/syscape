#ifndef SYSCAPE_DETAIL_GPU_COMMON_HPP
#define SYSCAPE_DETAIL_GPU_COMMON_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace gpu_common {

using vendor_type = ::syscape::gpu::gpu_vendor;

/// Converts one ASCII character to lowercase.
inline unsigned char ascii_lower(unsigned char value) noexcept {
    return value >= static_cast<unsigned char>('A') &&
                   value <= static_cast<unsigned char>('Z')
               ? static_cast<unsigned char>(
                     value + static_cast<unsigned char>('a' - 'A'))
               : value;
}

/// Checks whether an ASCII character belongs to an identifier-like word.
inline bool ascii_is_alphanumeric(unsigned char value) noexcept {
    return (value >= static_cast<unsigned char>('a') &&
            value <= static_cast<unsigned char>('z')) ||
           (value >= static_cast<unsigned char>('A') &&
            value <= static_cast<unsigned char>('Z')) ||
           (value >= static_cast<unsigned char>('0') &&
            value <= static_cast<unsigned char>('9'));
}

/// Checks if string `text` contains `needle` case-insensitively.
inline bool case_insensitive_contains(std::string_view text,
                                      std::string_view needle) noexcept {
    if (needle.empty()) { return true; }
    if (needle.size() > text.size()) { return false; }
    for (std::size_t i = 0; i <= text.size() - needle.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const auto c1 = static_cast<unsigned char>(text[i + j]);
            const auto c2 = static_cast<unsigned char>(needle[j]);
            if (ascii_lower(c1) != ascii_lower(c2)) {
                match = false;
                break;
            }
        }
        if (match) { return true; }
    }
    return false;
}

/// Checks for a case-insensitive ASCII token delimited by non-alphanumeric characters.
inline bool case_insensitive_contains_token(std::string_view text,
                                            std::string_view token) noexcept {
    if (token.empty()) { return false; }
    if (token.size() > text.size()) { return false; }
    for (std::size_t i = 0; i <= text.size() - token.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < token.size(); ++j) {
            const auto text_char = static_cast<unsigned char>(text[i + j]);
            const auto token_char = static_cast<unsigned char>(token[j]);
            if (ascii_lower(text_char) != ascii_lower(token_char)) {
                match = false;
                break;
            }
        }
        if (!match) { continue; }
        const bool begins_token =
            i == 0U ||
            !ascii_is_alphanumeric(static_cast<unsigned char>(text[i - 1U]));
        const std::size_t end = i + token.size();
        const bool ends_token =
            end == text.size() ||
            !ascii_is_alphanumeric(static_cast<unsigned char>(text[end]));
        if (begins_token && ends_token) { return true; }
    }
    return false;
}

/// Trims leading and trailing whitespace and null bytes.
inline std::string_view trim_whitespace(std::string_view input) noexcept {
    while (!input.empty() &&
           (input.back() == '\0' || input.back() == ' ' || input.back() == '\t' ||
            input.back() == '\r' || input.back() == '\n')) {
        input.remove_suffix(1U);
    }
    while (!input.empty() &&
           (input.front() == '\0' || input.front() == ' ' || input.front() == '\t' ||
            input.front() == '\r' || input.front() == '\n')) {
        input.remove_prefix(1U);
    }
    return input;
}

/// Parses a hexadecimal string (with optional "0x" or "0X" prefix) into a uint32_t.
inline result<std::uint32_t> parse_hex(std::string_view input) noexcept {
    std::string_view trimmed = trim_whitespace(input);
    if (trimmed.size() >= 2U && trimmed[0] == '0' &&
        (trimmed[1] == 'x' || trimmed[1] == 'X')) {
        trimmed.remove_prefix(2U);
    }
    if (trimmed.empty()) { return fail(errc::malformed_data); }

    std::uint32_t result_val = 0U;
    for (const char c : trimmed) {
        std::uint32_t digit = 0U;
        if (c >= '0' && c <= '9') {
            digit = static_cast<std::uint32_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            digit = static_cast<std::uint32_t>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            digit = static_cast<std::uint32_t>(c - 'A' + 10);
        } else {
            return fail(errc::malformed_data);
        }
        if (result_val > (0xFFFFFFFFU >> 4U)) {
            return fail(errc::value_too_large);
        }
        result_val = (result_val << 4U) | digit;
    }
    return result_val;
}

/// Classifies a PCI vendor ID to a gpu_vendor enum.
inline vendor_type classify_pci_vendor_id(std::uint32_t vendor_id) noexcept {
    switch (vendor_id) {
    case 0x1002U:
    case 0x1022U:
        return vendor_type::amd;
    case 0x10deU:
        return vendor_type::nvidia;
    case 0x8086U:
    case 0x8087U:
        return vendor_type::intel;
    case 0x106bU:
        return vendor_type::apple;
    case 0x13b5U:
        return vendor_type::arm_mali;
    case 0x5143U:
        return vendor_type::qualcomm_adreno;
    case 0x14e4U:
        return vendor_type::broadcom_videocore;
    case 0x1010U:
        return vendor_type::imagination_powervr;
    case 0x1414U:
        return vendor_type::microsoft;
    case 0x15adU:
        return vendor_type::vmware;
    case 0x1af4U:
        return vendor_type::virtio;
    case 0x1ab8U: // Parallels
    case 0x1b36U: // QEMU QXL
        return vendor_type::other;
    default:
        if (vendor_id != 0U) {
            return vendor_type::other;
        }
        return vendor_type::unknown;
    }
}

/// Returns the standard canonical vendor name for a classified gpu_vendor enum.
inline const char* vendor_to_string(vendor_type vendor) noexcept {
    switch (vendor) {
    case vendor_type::amd:
        return "AMD";
    case vendor_type::nvidia:
        return "NVIDIA";
    case vendor_type::intel:
        return "Intel";
    case vendor_type::apple:
        return "Apple";
    case vendor_type::arm_mali:
        return "ARM";
    case vendor_type::qualcomm_adreno:
        return "Qualcomm";
    case vendor_type::broadcom_videocore:
        return "Broadcom";
    case vendor_type::imagination_powervr:
        return "Imagination Technologies";
    case vendor_type::microsoft:
        return "Microsoft";
    case vendor_type::vmware:
        return "VMware";
    case vendor_type::virtio:
        return "Red Hat / VirtIO";
    case vendor_type::other:
        return "Other";
    case vendor_type::unknown:
    default:
        return "Unknown";
    }
}

/// Classifies a vendor name string into a gpu_vendor enum.
inline vendor_type classify_vendor_name(std::string_view name) noexcept {
    if (case_insensitive_contains(name, "nvidia")) {
        return vendor_type::nvidia;
    }
    if (case_insensitive_contains_token(name, "amd") ||
        case_insensitive_contains_token(name, "ati") ||
        case_insensitive_contains(name, "radeon") ||
        case_insensitive_contains(name, "advanced micro devices")) {
        return vendor_type::amd;
    }
    if (case_insensitive_contains(name, "intel")) {
        return vendor_type::intel;
    }
    if (case_insensitive_contains(name, "apple")) {
        return vendor_type::apple;
    }
    if (case_insensitive_contains(name, "mali") ||
        case_insensitive_contains(name, "immortalis") ||
        case_insensitive_contains_token(name, "arm")) {
        return vendor_type::arm_mali;
    }
    if (case_insensitive_contains(name, "adreno") ||
        case_insensitive_contains(name, "qualcomm")) {
        return vendor_type::qualcomm_adreno;
    }
    if (case_insensitive_contains(name, "videocore") ||
        case_insensitive_contains(name, "broadcom")) {
        return vendor_type::broadcom_videocore;
    }
    if (case_insensitive_contains(name, "powervr") ||
        case_insensitive_contains(name, "imagination")) {
        return vendor_type::imagination_powervr;
    }
    if (case_insensitive_contains(name, "microsoft") ||
        case_insensitive_contains(name, "basic render")) {
        return vendor_type::microsoft;
    }
    if (case_insensitive_contains(name, "vmware") ||
        case_insensitive_contains(name, "svga")) {
        return vendor_type::vmware;
    }
    if (case_insensitive_contains(name, "virtio") ||
        case_insensitive_contains(name, "red hat")) {
        return vendor_type::virtio;
    }
    if (!name.empty()) {
        return vendor_type::other;
    }
    return vendor_type::unknown;
}

} // namespace gpu_common
} // namespace detail
} // namespace syscape

#endif
