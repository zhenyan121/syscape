#ifndef SYSCAPE_DETAIL_WIFI_WINDOWS_PROFILE_HPP
#define SYSCAPE_DETAIL_WIFI_WINDOWS_PROFILE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <syscape/detail/utf8.hpp>
#include <syscape/detail/wifi/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>
#include <syscape/wifi.hpp>

namespace syscape {
namespace detail {
namespace wifi_backend {

inline std::optional<std::string_view>
xml_element(std::string_view xml, std::string_view tag) {
    const std::string open = "<" + std::string(tag) + ">";
    const std::string close = "</" + std::string(tag) + ">";
    const std::size_t begin = xml.find(open);
    if (begin == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t content_begin = begin + open.size();
    const std::size_t end = xml.find(close, content_begin);
    if (end == std::string_view::npos) {
        return std::nullopt;
    }
    return xml.substr(content_begin, end - content_begin);
}

inline result<std::string> decode_xml_text(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    std::size_t offset = 0U;
    while (offset < value.size()) {
        if (value[offset] != '&') {
            decoded.push_back(value[offset++]);
            continue;
        }
        const std::size_t end = value.find(';', offset + 1U);
        if (end == std::string_view::npos) {
            return fail(errc::malformed_data);
        }
        const std::string_view entity =
            value.substr(offset + 1U, end - offset - 1U);
        if (entity == "amp") decoded.push_back('&');
        else if (entity == "lt") decoded.push_back('<');
        else if (entity == "gt") decoded.push_back('>');
        else if (entity == "quot") decoded.push_back('"');
        else if (entity == "apos") decoded.push_back('\'');
        else if (!entity.empty() && entity.front() == '#') {
            int base = 10;
            std::string_view digits = entity.substr(1U);
            if (!digits.empty() &&
                (digits.front() == 'x' || digits.front() == 'X')) {
                base = 16;
                digits.remove_prefix(1U);
            }
            const auto code_point =
                wifi_common::parse_int<std::uint32_t>(digits, base);
            if (!code_point || *code_point == 0U ||
                *code_point > 0x10FFFFU ||
                (*code_point >= 0xD800U && *code_point <= 0xDFFFU)) {
                return fail(errc::invalid_encoding);
            }
            append_utf8(decoded, static_cast<char32_t>(*code_point));
        } else {
            return fail(errc::malformed_data);
        }
        offset = end + 1U;
    }
    if (!is_valid_utf8(decoded)) {
        return fail(errc::invalid_encoding);
    }
    return decoded;
}

inline result<bool> parse_profile_xml(std::string_view xml,
                                      wifi::configured_network& profile) {
    const auto ssid_config = xml_element(xml, "SSIDConfig");
    if (!ssid_config) {
        return fail(errc::malformed_data);
    }
    const auto ssid_element = xml_element(*ssid_config, "SSID");
    if (!ssid_element) {
        return fail(errc::malformed_data);
    }
    const auto hex_ssid = xml_element(*ssid_element, "hex");
    if (hex_ssid) {
        const std::string_view hex = wifi_common::trim_whitespace(*hex_ssid);
        if (hex.empty() || hex.size() % 2U != 0U) {
            return fail(errc::malformed_data);
        }
        std::string decoded;
        decoded.reserve(hex.size() / 2U);
        for (std::size_t index = 0U; index < hex.size(); index += 2U) {
            const auto byte = wifi_common::parse_int<std::uint8_t>(
                hex.substr(index, 2U), 16);
            if (!byte) {
                return fail(errc::malformed_data);
            }
            decoded.push_back(static_cast<char>(*byte));
        }
        if (!is_valid_utf8(decoded)) {
            return fail(errc::invalid_encoding);
        }
        profile.ssid = std::move(decoded);
    } else {
        const auto name = xml_element(*ssid_element, "name");
        if (!name) {
            return fail(errc::malformed_data);
        }
        const auto decoded = decode_xml_text(*name);
        if (!decoded) {
            return fail(decoded.error());
        }
        profile.ssid = *decoded;
    }

    std::optional<std::string_view> authentication;
    const auto msm = xml_element(xml, "MSM");
    if (msm) {
        const auto security = xml_element(*msm, "security");
        if (security) {
            const auto auth_encryption =
                xml_element(*security, "authEncryption");
            if (auth_encryption) {
                authentication =
                    xml_element(*auth_encryption, "authentication");
            }
        }
    }
    if (authentication) {
        const std::string_view auth =
            wifi_common::trim_whitespace(*authentication);
        if (auth == "open") {
            profile.security = wifi::security_type::open;
        } else if (auth == "shared") {
            profile.security = wifi::security_type::wep;
        } else if (auth == "WPA") {
            profile.security = wifi::security_type::wpa_enterprise;
        } else if (auth == "WPAPSK") {
            profile.security = wifi::security_type::wpa_personal;
        } else if (auth == "WPA2") {
            profile.security = wifi::security_type::wpa2_enterprise;
        } else if (auth == "WPA2PSK") {
            profile.security = wifi::security_type::wpa2_personal;
        } else if (auth == "WPA3SAE") {
            profile.security = wifi::security_type::wpa3_personal;
        } else if (auth == "WPA3" || auth == "WPA3ENT" ||
                   auth == "WPA3ENT192") {
            profile.security = wifi::security_type::wpa3_enterprise;
        } else if (auth == "OWE") {
            profile.security = wifi::security_type::wpa3_owe;
        }
    }

    const auto connection_mode = xml_element(xml, "connectionMode");
    profile.auto_connect = !connection_mode ||
        wifi_common::trim_whitespace(*connection_mode) == "auto";
    const auto non_broadcast = xml_element(*ssid_config, "nonBroadcast");
    profile.is_hidden = non_broadcast &&
        wifi_common::trim_whitespace(*non_broadcast) == "true";
    return true;
}

} // namespace wifi_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_WIFI_WINDOWS_PROFILE_HPP
