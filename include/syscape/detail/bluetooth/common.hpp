#ifndef SYSCAPE_DETAIL_BLUETOOTH_COMMON_HPP
#define SYSCAPE_DETAIL_BLUETOOTH_COMMON_HPP

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/bluetooth.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace bluetooth_common {

/// Trims leading and trailing ASCII whitespace.
inline std::string_view trim_whitespace(std::string_view text) noexcept {
    while (!text.empty() && static_cast<unsigned char>(text.front()) <= ' ') {
        text.remove_prefix(1U);
    }
    while (!text.empty() && static_cast<unsigned char>(text.back()) <= ' ') {
        text.remove_suffix(1U);
    }
    return text;
}

/// Compares text naturally so that decimal digit runs sort by numeric value.
inline bool natural_less(std::string_view left,
                         std::string_view right) noexcept {
    std::size_t left_pos = 0U;
    std::size_t right_pos = 0U;
    while (left_pos < left.size() && right_pos < right.size()) {
        const bool left_digit = left[left_pos] >= '0' && left[left_pos] <= '9';
        const bool right_digit =
            right[right_pos] >= '0' && right[right_pos] <= '9';
        if (!left_digit || !right_digit) {
            if (left[left_pos] != right[right_pos]) {
                return left[left_pos] < right[right_pos];
            }
            ++left_pos;
            ++right_pos;
            continue;
        }

        const std::size_t left_run_begin = left_pos;
        const std::size_t right_run_begin = right_pos;
        while (left_pos < left.size() && left[left_pos] == '0') {
            ++left_pos;
        }
        while (right_pos < right.size() && right[right_pos] == '0') {
            ++right_pos;
        }
        const std::size_t left_significant = left_pos;
        const std::size_t right_significant = right_pos;
        while (left_pos < left.size() && left[left_pos] >= '0' &&
               left[left_pos] <= '9') {
            ++left_pos;
        }
        while (right_pos < right.size() && right[right_pos] >= '0' &&
               right[right_pos] <= '9') {
            ++right_pos;
        }

        const std::size_t left_digits = left_pos - left_significant;
        const std::size_t right_digits = right_pos - right_significant;
        if (left_digits != right_digits) {
            return left_digits < right_digits;
        }
        const int numeric_order =
            left.substr(left_significant, left_digits)
                .compare(right.substr(right_significant, right_digits));
        if (numeric_order != 0) {
            return numeric_order < 0;
        }
        const std::size_t left_leading_zeros =
            left_significant - left_run_begin;
        const std::size_t right_leading_zeros =
            right_significant - right_run_begin;
        if (left_leading_zeros != right_leading_zeros) {
            return left_leading_zeros < right_leading_zeros;
        }
    }
    return left_pos >= left.size() && right_pos < right.size();
}

/// Parses an unsigned integer in the given radix (default 10).
template <typename IntType>
inline std::optional<IntType> parse_int(std::string_view text,
                                        int base = 10) noexcept {
    text = trim_whitespace(text);
    if (base == 16) {
        if (text.size() >= 2U && text[0] == '0' &&
            (text[1] == 'x' || text[1] == 'X')) {
            text.remove_prefix(2U);
        }
    }
    if (text.empty()) {
        return std::nullopt;
    }
    IntType value{};
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto res = std::from_chars(begin, end, value, base);
    if (res.ec == std::errc{} && res.ptr == end) {
        return value;
    }
    return std::nullopt;
}

/// Normalizes a 6-byte Bluetooth MAC address to uppercase colon-separated
/// format "XX:XX:XX:XX:XX:XX".
inline std::optional<std::string>
normalize_mac_address(std::string_view text) {
    text = trim_whitespace(text);
    std::string hex_digits;
    hex_digits.reserve(12U);
    for (char c : text) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            hex_digits.push_back(
                static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        } else if (c != ':' && c != '-' && c != '.') {
            return std::nullopt;
        }
    }
    if (hex_digits.size() != 12U) {
        return std::nullopt;
    }
    std::string result;
    result.reserve(17U);
    for (std::size_t i = 0U; i < 12U; i += 2U) {
        if (i > 0U) {
            result.push_back(':');
        }
        result.push_back(hex_digits[i]);
        result.push_back(hex_digits[i + 1U]);
    }
    return result;
}

/// Formats 6 raw bytes into an uppercase colon-separated Bluetooth MAC address.
inline std::string format_mac_bytes(const std::uint8_t* bytes,
                                    bool little_endian = false) {
    static constexpr char hex_chars[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(17U);
    for (int i = 0; i < 6; ++i) {
        const int idx = little_endian ? (5 - i) : i;
        const auto b = bytes[idx];
        if (i > 0) {
            result.push_back(':');
        }
        result.push_back(hex_chars[(b >> 4) & 0x0F]);
        result.push_back(hex_chars[b & 0x0F]);
    }
    return result;
}

/// Decodes the major device class from a 24-bit Bluetooth Class of Device
/// (CoD).
inline bluetooth::major_device_class
decode_major_device_class(std::uint32_t class_of_device) noexcept {
    // Bits 8-12 encode Major Device Class
    const std::uint32_t major = (class_of_device >> 8) & 0x1FU;
    switch (major) {
    case 0x00:
        return bluetooth::major_device_class::miscellaneous;
    case 0x01:
        return bluetooth::major_device_class::computer;
    case 0x02:
        return bluetooth::major_device_class::phone;
    case 0x03:
        return bluetooth::major_device_class::network_access_point;
    case 0x04:
        return bluetooth::major_device_class::audio_video;
    case 0x05:
        return bluetooth::major_device_class::peripheral;
    case 0x06:
        return bluetooth::major_device_class::imaging;
    case 0x07:
        return bluetooth::major_device_class::wearable;
    case 0x08:
        return bluetooth::major_device_class::toy;
    case 0x09:
        return bluetooth::major_device_class::health;
    default:
        return bluetooth::major_device_class::unknown;
    }
}

} // namespace bluetooth_common
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_BLUETOOTH_COMMON_HPP
