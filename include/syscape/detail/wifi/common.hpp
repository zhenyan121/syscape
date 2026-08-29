#ifndef SYSCAPE_DETAIL_WIFI_COMMON_HPP
#define SYSCAPE_DETAIL_WIFI_COMMON_HPP

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
#include <syscape/wifi.hpp>

namespace syscape {
namespace detail {
namespace wifi_common {

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
    const auto* const first = text.data();
    const auto* const last = first + text.size();
    const auto parse_result = std::from_chars(first, last, value, base);
    if (parse_result.ec != std::errc{} || parse_result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

/// Parses a signed integer in base 10.
template <typename IntType>
inline std::optional<IntType> parse_signed_int(std::string_view text) noexcept {
    text = trim_whitespace(text);
    if (text.empty()) {
        return std::nullopt;
    }
    IntType value{};
    const auto* const first = text.data();
    const auto* const last = first + text.size();
    const auto parse_result = std::from_chars(first, last, value, 10);
    if (parse_result.ec != std::errc{} || parse_result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

/// Converts a 6-byte raw MAC address to standard "AA:BB:CC:DD:EE:FF" format.
inline std::string format_mac_bytes(const std::uint8_t* bytes) {
    static constexpr char hex_digits[] = "0123456789ABCDEF";
    std::string formatted;
    formatted.reserve(17U);
    for (std::size_t i = 0; i < 6U; ++i) {
        if (i > 0) {
            formatted.push_back(':');
        }
        const std::uint8_t byte = bytes[i];
        formatted.push_back(hex_digits[(byte >> 4) & 0x0F]);
        formatted.push_back(hex_digits[byte & 0x0F]);
    }
    return formatted;
}

/// Normalizes a MAC address string to standard uppercase colon-separated
/// format "AA:BB:CC:DD:EE:FF".
inline std::optional<std::string>
normalize_mac_address(std::string_view raw) {
    raw = trim_whitespace(raw);
    std::uint8_t bytes[6] = {0};
    std::size_t byte_index = 0;

    if (raw.size() == 17U && (raw[2] == ':' || raw[2] == '-')) {
        const char sep = raw[2];
        for (std::size_t i = 0; i < 6U; ++i) {
            if (i > 0 && raw[i * 3U - 1U] != sep) {
                return std::nullopt;
            }
            const auto hi = parse_int<std::uint8_t>(raw.substr(i * 3U, 1U), 16);
            const auto lo =
                parse_int<std::uint8_t>(raw.substr(i * 3U + 1U, 1U), 16);
            if (!hi || !lo) {
                return std::nullopt;
            }
            bytes[byte_index++] = static_cast<std::uint8_t>((*hi << 4) | *lo);
        }
    } else if (raw.size() == 12U) {
        for (std::size_t i = 0; i < 6U; ++i) {
            const auto hi = parse_int<std::uint8_t>(raw.substr(i * 2U, 1U), 16);
            const auto lo =
                parse_int<std::uint8_t>(raw.substr(i * 2U + 1U, 1U), 16);
            if (!hi || !lo) {
                return std::nullopt;
            }
            bytes[byte_index++] = static_cast<std::uint8_t>((*hi << 4) | *lo);
        }
    } else {
        return std::nullopt;
    }

    return format_mac_bytes(bytes);
}

/// Classifies frequency into standard Wi-Fi frequency band.
inline wifi::frequency_band frequency_to_band(std::uint32_t freq_mhz) noexcept {
    if (freq_mhz >= 2400U && freq_mhz <= 2500U) {
        return wifi::frequency_band::band_2_4_ghz;
    }
    if (freq_mhz >= 4900U && freq_mhz <= 5895U) {
        return wifi::frequency_band::band_5_ghz;
    }
    if (freq_mhz >= 5925U && freq_mhz <= 7125U) {
        return wifi::frequency_band::band_6_ghz;
    }
    if (freq_mhz >= 57000U && freq_mhz <= 71000U) {
        return wifi::frequency_band::band_60_ghz;
    }
    return wifi::frequency_band::unknown;
}

/// Derives standard Wi-Fi channel number from frequency in MHz.
inline std::optional<std::uint16_t>
frequency_to_channel(std::uint32_t freq_mhz) noexcept {
    if (freq_mhz >= 2412U && freq_mhz <= 2472U) {
        if ((freq_mhz - 2407U) % 5U == 0U) {
            return static_cast<std::uint16_t>((freq_mhz - 2407U) / 5U);
        }
    } else if (freq_mhz == 2484U) {
        return static_cast<std::uint16_t>(14U);
    } else if (freq_mhz >= 5000U && freq_mhz <= 5895U) {
        if ((freq_mhz - 5000U) % 5U == 0U) {
            return static_cast<std::uint16_t>((freq_mhz - 5000U) / 5U);
        }
    } else if (freq_mhz >= 5955U && freq_mhz <= 7115U) {
        if ((freq_mhz - 5950U) % 5U == 0U) {
            return static_cast<std::uint16_t>((freq_mhz - 5950U) / 5U);
        }
    }
    return std::nullopt;
}

/// Estimates signal quality percentage (0 - 100) from RSSI in dBm.
inline std::uint8_t rssi_to_quality_percent(std::int16_t rssi_dbm) noexcept {
    if (rssi_dbm <= -100) {
        return 0U;
    }
    if (rssi_dbm >= -50) {
        return 100U;
    }
    return static_cast<std::uint8_t>(2 * (rssi_dbm + 100));
}

/// Selects an adapter only when the available data establishes one
/// unambiguous primary candidate.
inline result<wifi::adapter_info>
select_default_adapter(const std::vector<wifi::adapter_info>& adapters) {
    if (adapters.empty()) {
        return fail(errc::not_found);
    }
    if (adapters.size() == 1U) {
        return adapters.front();
    }
    const wifi::adapter_info* selected = nullptr;
    for (const auto& adapter : adapters) {
        if (adapter.state == wifi::connection_state::connected ||
            adapter.connection.has_value()) {
            if (selected != nullptr) {
                return fail(errc::not_supported);
            }
            selected = &adapter;
        }
    }
    if (selected == nullptr) {
        return fail(errc::not_supported);
    }
    return *selected;
}

} // namespace wifi_common
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_WIFI_COMMON_HPP
