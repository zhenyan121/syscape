#ifndef SYSCAPE_DETAIL_DISPLAY_COMMON_HPP
#define SYSCAPE_DETAIL_DISPLAY_COMMON_HPP

#include <cstddef>
#include <cstdint>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace display_common {

constexpr std::size_t edid_block_size = 128U;
constexpr std::size_t maximum_edid_size = edid_block_size * 256U;

/// Converts one ASCII character to lowercase.
inline unsigned char ascii_lower(unsigned char value) noexcept {
    return value >= static_cast<unsigned char>('A') &&
                   value <= static_cast<unsigned char>('Z')
               ? static_cast<unsigned char>(
                     value + static_cast<unsigned char>('a' - 'A'))
               : value;
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

/// Parses a mode string in the form "WIDTHxHEIGHT" (e.g. "2560x1600", "1920x1080").
inline result<std::pair<std::uint32_t, std::uint32_t>> parse_mode_resolution(
    std::string_view input) noexcept {
    const std::string_view trimmed = trim_whitespace(input);
    const std::size_t x_pos = trimmed.find_first_of("xX");
    if (x_pos == std::string_view::npos || x_pos == 0U || x_pos == trimmed.size() - 1U) {
        return fail(errc::malformed_data);
    }

    std::uint32_t width = 0U;
    const char* w_first = trimmed.data();
    const char* w_last = w_first + x_pos;
    const auto w_res = std::from_chars(w_first, w_last, width);
    if (w_res.ec != std::errc() || w_res.ptr != w_last || width == 0U) {
        return fail(errc::malformed_data);
    }

    std::uint32_t height = 0U;
    const char* h_first = trimmed.data() + x_pos + 1U;
    const char* h_last = trimmed.data() + trimmed.size();
    const auto h_res = std::from_chars(h_first, h_last, height);
    if (h_res.ec != std::errc() || h_res.ptr != h_last || height == 0U) {
        return fail(errc::malformed_data);
    }

    return std::make_pair(width, height);
}

/// Parsed facts from a standard VESA EDID 1.3 / 1.4 block (128 bytes).
struct edid_facts {
    /// 3-letter PNP manufacturer ID (e.g. "DEL", "SAM", "TMA").
    std::optional<std::string> manufacturer;
    /// Product ID / product code.
    std::optional<std::uint16_t> product_code;
    /// User-friendly monitor name from descriptor tag 0xFC, if present.
    std::optional<std::string> monitor_name;
    /// Physical screen width in millimeters.
    std::optional<std::uint32_t> physical_width_mm;
    /// Physical screen height in millimeters.
    std::optional<std::uint32_t> physical_height_mm;
    /// Preferred native resolution width in pixels from detailed timing descriptor.
    std::optional<std::uint32_t> preferred_width;
    /// Preferred native resolution height in pixels from detailed timing descriptor.
    std::optional<std::uint32_t> preferred_height;
    /// Preferred refresh rate in Hz from detailed timing descriptor.
    std::optional<double> preferred_refresh_rate_hz;
};

/// Parses a standard 128-byte VESA EDID block (or longer binary buffer).
inline result<edid_facts> parse_edid_block(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < edid_block_size ||
        size > maximum_edid_size || size % edid_block_size != 0U) {
        return fail(errc::malformed_data);
    }

    // Validate standard EDID header: 00 FF FF FF FF FF FF 00
    constexpr std::uint8_t expected_header[8] = {
        0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x00U
    };
    for (std::size_t i = 0; i < 8U; ++i) {
        if (data[i] != expected_header[i]) {
            return fail(errc::malformed_data);
        }
    }

    const std::size_t block_count = static_cast<std::size_t>(data[126]) + 1U;
    if (size != block_count * edid_block_size) {
        return fail(errc::malformed_data);
    }

    // Every advertised 128-byte EDID block has an 8-bit checksum whose byte
    // sum is zero, including CEA and DisplayID extension blocks.
    for (std::size_t block = 0U; block < block_count; ++block) {
        std::uint8_t checksum = 0U;
        const std::size_t offset = block * edid_block_size;
        for (std::size_t i = 0U; i < edid_block_size; ++i) {
            checksum = static_cast<std::uint8_t>(checksum + data[offset + i]);
        }
        if (checksum != 0U) {
            return fail(errc::malformed_data);
        }
    }

    edid_facts facts;

    // 1. Manufacturer ID at bytes 8-9 (big-endian 16-bit with 5-bit character packing)
    const std::uint16_t mfg_raw = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[8]) << 8U) | static_cast<std::uint16_t>(data[9]));
    const char c1 = static_cast<char>(((mfg_raw >> 10U) & 0x1FU) + 'A' - 1);
    const char c2 = static_cast<char>(((mfg_raw >> 5U) & 0x1FU) + 'A' - 1);
    const char c3 = static_cast<char>((mfg_raw & 0x1FU) + 'A' - 1);
    if (c1 >= 'A' && c1 <= 'Z' && c2 >= 'A' && c2 <= 'Z' && c3 >= 'A' && c3 <= 'Z') {
        std::string mfg;
        mfg.reserve(3U);
        mfg.push_back(c1);
        mfg.push_back(c2);
        mfg.push_back(c3);
        facts.manufacturer = std::move(mfg);
    }

    // 2. Product code at bytes 10-11 (little-endian 16-bit)
    const std::uint16_t prod_code = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[10]) | (static_cast<std::uint16_t>(data[11]) << 8U));
    if (prod_code != 0U) {
        facts.product_code = prod_code;
    }

    // 3. Physical screen size at bytes 21 (horizontal cm) and 22 (vertical cm)
    const std::uint8_t h_cm = data[21];
    const std::uint8_t v_cm = data[22];
    if (h_cm > 0U && v_cm > 0U) {
        facts.physical_width_mm = static_cast<std::uint32_t>(h_cm) * 10U;
        facts.physical_height_mm = static_cast<std::uint32_t>(v_cm) * 10U;
    }

    // 4. Descriptor blocks: 4 blocks of 18 bytes starting at offset 54 (0x36)
    constexpr std::size_t descriptor_offsets[4] = { 54U, 72U, 90U, 108U };
    for (std::size_t d = 0; d < 4U; ++d) {
        const std::size_t off = descriptor_offsets[d];
        const std::uint8_t* blk = data + off;

        // Check if it is a display descriptor (bytes 0-1 are 0x00 0x00 and byte 2 is 0x00)
        if (blk[0] == 0x00U && blk[1] == 0x00U && blk[2] == 0x00U) {
            const std::uint8_t tag = blk[3];
            if (tag == 0xFCU) {
                // Monitor Name descriptor (bytes 5 to 17, up to 13 bytes ASCII)
                const char* name_ptr = reinterpret_cast<const char*>(blk + 5U);
                std::string_view name_sv(name_ptr, 13U);
                std::string name(trim_whitespace(name_sv));
                if (!name.empty()) {
                    if (!is_valid_utf8(name)) {
                        return fail(errc::invalid_encoding);
                    }
                    facts.monitor_name = std::move(name);
                }
            }
        } else {
            // Detailed Timing Descriptor
            const std::uint32_t pix_clock_10khz = static_cast<std::uint32_t>(
                static_cast<std::uint32_t>(blk[0]) | (static_cast<std::uint32_t>(blk[1]) << 8U));
            if (pix_clock_10khz > 0U && !facts.preferred_width.has_value()) {
                const std::uint32_t h_active = static_cast<std::uint32_t>(
                    static_cast<std::uint32_t>(blk[2]) |
                    ((static_cast<std::uint32_t>(blk[4]) & 0xF0U) << 4U));
                const std::uint32_t h_blanking = static_cast<std::uint32_t>(
                    static_cast<std::uint32_t>(blk[3]) |
                    ((static_cast<std::uint32_t>(blk[4]) & 0x0FU) << 8U));
                const std::uint32_t v_active = static_cast<std::uint32_t>(
                    static_cast<std::uint32_t>(blk[5]) |
                    ((static_cast<std::uint32_t>(blk[7]) & 0xF0U) << 4U));
                const std::uint32_t v_blanking = static_cast<std::uint32_t>(
                    static_cast<std::uint32_t>(blk[6]) |
                    ((static_cast<std::uint32_t>(blk[7]) & 0x0FU) << 8U));

                const std::uint32_t h_total = h_active + h_blanking;
                const std::uint32_t v_total = v_active + v_blanking;

                if (h_active > 0U && v_active > 0U && h_total > 0U && v_total > 0U) {
                    facts.preferred_width = h_active;
                    facts.preferred_height = v_active;
                    const double pix_clock_hz = static_cast<double>(pix_clock_10khz) * 10000.0;
                    facts.preferred_refresh_rate_hz =
                        pix_clock_hz / (static_cast<double>(h_total) * static_cast<double>(v_total));
                }
            }
        }
    }

    return facts;
}

/// Parses connector type and internal flag from a connector name like "card0-DP-1", "card1-eDP-1", "HDMI-A-1".
inline void classify_connector_name(
    std::string_view connector_name,
    std::optional<std::string>& out_connector_type,
    bool& out_is_internal) {
    // Strip leading "cardN-" if present
    std::string_view name = connector_name;
    if (name.substr(0, 4) == "card") {
        const std::size_t dash = name.find('-');
        if (dash != std::string_view::npos && dash + 1U < name.size()) {
            name = name.substr(dash + 1U);
        }
    }

    // Find the connector type substring (e.g. "eDP", "DP", "HDMI-A", "VGA", "DVI-I", "DVI-D")
    // Usually ends before the last hyphen and numeric index, e.g. "DP-1" -> "DP", "HDMI-A-1" -> "HDMI-A", "eDP-2" -> "eDP"
    std::string_view type_part = name;
    const std::size_t last_dash = name.find_last_of('-');
    if (last_dash != std::string_view::npos && last_dash > 0U) {
        // Verify remainder after last dash is numeric
        bool is_num = true;
        for (std::size_t i = last_dash + 1U; i < name.size(); ++i) {
            if (name[i] < '0' || name[i] > '9') {
                is_num = false;
                break;
            }
        }
        if (is_num) {
            type_part = name.substr(0, last_dash);
        }
    }

    out_connector_type = std::string(type_part);

    // Classify internal panel types
    if (type_part == "eDP" || type_part == "LVDS" || type_part == "DSI" ||
        type_part == "edp" || type_part == "lvds" || type_part == "dsi") {
        out_is_internal = true;
    } else {
        out_is_internal = false;
    }
}

} // namespace display_common
} // namespace detail
} // namespace syscape

#endif
