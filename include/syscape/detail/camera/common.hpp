#ifndef SYSCAPE_DETAIL_CAMERA_COMMON_HPP
#define SYSCAPE_DETAIL_CAMERA_COMMON_HPP

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace camera_common {

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

/// Case-insensitive substring search.
inline bool contains_ignore_case(std::string_view text,
                                 std::string_view needle) noexcept {
    if (needle.empty()) {
        return true;
    }
    if (text.size() < needle.size()) {
        return false;
    }
    for (std::size_t i = 0; i <= text.size() - needle.size(); ++i) {
        bool matches = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const auto a = static_cast<unsigned char>(text[i + j]);
            const auto b = static_cast<unsigned char>(needle[j]);
            if (std::tolower(a) != std::tolower(b)) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }
    return false;
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

        const std::size_t left_run_size = left_pos - left_run_begin;
        const std::size_t right_run_size = right_pos - right_run_begin;
        if (left_run_size != right_run_size) {
            return left_run_size < right_run_size;
        }
    }
    return left.size() < right.size();
}

/// Parses an unsigned 32-bit decimal integer with bounds checking.
inline result<std::uint32_t> parse_u32(std::string_view input) noexcept {
    const std::string_view trimmed = trim_whitespace(input);
    if (trimmed.empty()) {
        return fail(errc::malformed_data);
    }
    std::uint32_t value = 0U;
    const char* first = trimmed.data();
    const char* last = first + trimmed.size();
    const auto res = std::from_chars(first, last, value);
    if (res.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (res.ec != std::errc() || res.ptr != last) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Parses a 16-bit hex integer (e.g. "5986", "04f2").
inline result<std::uint16_t> parse_hex_u16(std::string_view input) noexcept {
    std::string_view trimmed = trim_whitespace(input);
    if (trimmed.rfind("0x", 0) == 0 || trimmed.rfind("0X", 0) == 0) {
        trimmed.remove_prefix(2U);
    }
    if (trimmed.empty()) {
        return fail(errc::malformed_data);
    }
    std::uint16_t value = 0U;
    const char* first = trimmed.data();
    const char* last = first + trimmed.size();
    const auto res = std::from_chars(first, last, value, 16);
    if (res.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (res.ec != std::errc() || res.ptr != last) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Filters a list of camera devices to only those capable of video capture.
inline std::vector<::syscape::camera::camera_device> filter_capture_devices(
    const std::vector<::syscape::camera::camera_device>& devices) {
    std::vector<::syscape::camera::camera_device> filtered;
    filtered.reserve(devices.size());
    for (const auto& dev : devices) {
        if (dev.capabilities.has_value() &&
            dev.capabilities->has_video_capture.value_or(false)) {
            filtered.push_back(dev);
        }
    }
    return filtered;
}

} // namespace camera_common
} // namespace detail
} // namespace syscape

#endif
