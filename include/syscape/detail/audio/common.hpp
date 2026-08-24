#ifndef SYSCAPE_DETAIL_AUDIO_COMMON_HPP
#define SYSCAPE_DETAIL_AUDIO_COMMON_HPP

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/audio.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace audio_common {

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
inline bool contains_ignore_case(
    std::string_view text, std::string_view needle) noexcept {
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

/// Filters a list of audio devices by stream direction (playback or capture).
/// A duplex device matches both playback and capture queries.
inline std::vector<::syscape::audio::audio_device> filter_by_direction(
    const std::vector<::syscape::audio::audio_device>& devices,
    ::syscape::audio::audio_device_direction target_dir) {
    std::vector<::syscape::audio::audio_device> filtered;
    filtered.reserve(devices.size());
    for (const auto& dev : devices) {
        if (dev.direction == target_dir ||
            dev.direction == ::syscape::audio::audio_device_direction::duplex) {
            filtered.push_back(dev);
        }
    }
    return filtered;
}

/// Finds the default device for a given direction from a list of devices.
inline result<::syscape::audio::audio_device> find_default_device(
    const std::vector<::syscape::audio::audio_device>& devices,
    ::syscape::audio::audio_device_direction target_dir) {
    for (const auto& dev : devices) {
        const bool matches_direction =
            dev.direction == target_dir ||
            dev.direction == ::syscape::audio::audio_device_direction::duplex;
        const bool is_default =
            target_dir == ::syscape::audio::audio_device_direction::playback
                ? dev.is_default_playback.value_or(false)
                : dev.is_default_capture.value_or(false);
        if (matches_direction && is_default) {
            return dev;
        }
    }
    return fail(errc::not_found);
}

} // namespace audio_common
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_AUDIO_COMMON_HPP
