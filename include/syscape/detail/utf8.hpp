#ifndef SYSCAPE_DETAIL_UTF8_HPP
#define SYSCAPE_DETAIL_UTF8_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {

struct decoded_code_point {
    char32_t value;
    std::size_t length;
};

inline result<decoded_code_point> decode_utf8_code_point(
    std::string_view input, std::size_t offset) {
    if (offset >= input.size()) {
        return fail(errc::invalid_encoding);
    }

    const auto byte = [&input](std::size_t index) noexcept {
        return static_cast<unsigned char>(input[index]);
    };
    const auto continuation = [](unsigned char value) noexcept {
        return value >= 0x80U && value <= 0xBFU;
    };

    const unsigned char first = byte(offset);
    if (first <= 0x7FU) {
        return decoded_code_point{static_cast<char32_t>(first), 1U};
    }

    if (first >= 0xC2U && first <= 0xDFU) {
        if (offset + 1U >= input.size() || !continuation(byte(offset + 1U))) {
            return fail(errc::invalid_encoding);
        }
        const char32_t value =
            (static_cast<char32_t>(first & 0x1FU) << 6U) |
            static_cast<char32_t>(byte(offset + 1U) & 0x3FU);
        return decoded_code_point{value, 2U};
    }

    if (first >= 0xE0U && first <= 0xEFU) {
        if (offset + 2U >= input.size()) {
            return fail(errc::invalid_encoding);
        }
        const unsigned char second = byte(offset + 1U);
        const unsigned char third = byte(offset + 2U);
        const bool second_valid =
            (first == 0xE0U) ? (second >= 0xA0U && second <= 0xBFU) :
            (first == 0xEDU) ? (second >= 0x80U && second <= 0x9FU) :
                               continuation(second);
        if (!second_valid || !continuation(third)) {
            return fail(errc::invalid_encoding);
        }
        const char32_t value =
            (static_cast<char32_t>(first & 0x0FU) << 12U) |
            (static_cast<char32_t>(second & 0x3FU) << 6U) |
            static_cast<char32_t>(third & 0x3FU);
        return decoded_code_point{value, 3U};
    }

    if (first >= 0xF0U && first <= 0xF4U) {
        if (offset + 3U >= input.size()) {
            return fail(errc::invalid_encoding);
        }
        const unsigned char second = byte(offset + 1U);
        const unsigned char third = byte(offset + 2U);
        const unsigned char fourth = byte(offset + 3U);
        const bool second_valid =
            (first == 0xF0U) ? (second >= 0x90U && second <= 0xBFU) :
            (first == 0xF4U) ? (second >= 0x80U && second <= 0x8FU) :
                               continuation(second);
        if (!second_valid || !continuation(third) || !continuation(fourth)) {
            return fail(errc::invalid_encoding);
        }
        const char32_t value =
            (static_cast<char32_t>(first & 0x07U) << 18U) |
            (static_cast<char32_t>(second & 0x3FU) << 12U) |
            (static_cast<char32_t>(third & 0x3FU) << 6U) |
            static_cast<char32_t>(fourth & 0x3FU);
        return decoded_code_point{value, 4U};
    }

    return fail(errc::invalid_encoding);
}

inline bool is_valid_utf8(std::string_view input) noexcept {
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const result<decoded_code_point> decoded =
            decode_utf8_code_point(input, offset);
        if (!decoded) {
            return false;
        }
        offset += decoded->length;
    }
    return true;
}

inline result<std::u16string> utf8_to_utf16(std::string_view input) {
    std::u16string output;
    output.reserve(input.size());

    std::size_t offset = 0U;
    while (offset < input.size()) {
        const result<decoded_code_point> decoded =
            decode_utf8_code_point(input, offset);
        if (!decoded) {
            return fail(decoded.error());
        }

        const char32_t value = decoded->value;
        if (value <= 0xFFFFU) {
            output.push_back(static_cast<char16_t>(value));
        } else {
            const char32_t adjusted = value - 0x10000U;
            output.push_back(static_cast<char16_t>(0xD800U + (adjusted >> 10U)));
            output.push_back(static_cast<char16_t>(0xDC00U + (adjusted & 0x3FFU)));
        }
        offset += decoded->length;
    }

    return output;
}

inline void append_utf8(std::string& output, char32_t value) {
    if (value <= 0x7FU) {
        output.push_back(static_cast<char>(value));
    } else if (value <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (value >> 6U)));
        output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    } else if (value <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (value >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (value >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((value >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((value >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
    }
}

inline result<std::string> utf16_to_utf8(std::u16string_view input) {
    std::string output;
    output.reserve(input.size());

    std::size_t index = 0U;
    while (index < input.size()) {
        const char32_t first = input[index++];
        char32_t value = first;

        if (first >= 0xD800U && first <= 0xDBFFU) {
            if (index >= input.size()) {
                return fail(errc::invalid_encoding);
            }
            const char32_t second = input[index++];
            if (second < 0xDC00U || second > 0xDFFFU) {
                return fail(errc::invalid_encoding);
            }
            value = 0x10000U + ((first - 0xD800U) << 10U) +
                    (second - 0xDC00U);
        } else if (first >= 0xDC00U && first <= 0xDFFFU) {
            return fail(errc::invalid_encoding);
        }

        append_utf8(output, value);
    }

    return output;
}

} // namespace detail
} // namespace syscape

#endif
