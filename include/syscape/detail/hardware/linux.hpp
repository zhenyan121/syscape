#ifndef SYSCAPE_DETAIL_HARDWARE_LINUX_HPP
#define SYSCAPE_DETAIL_HARDWARE_LINUX_HPP

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <system_error>

#include <syscape/detail/hardware/common.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_backend {

/// Root of the kernel's DMI-id class interface.
///
/// The attributes read here follow Documentation/ABI/testing/sysfs-class-dmi,
/// which the kernel documents but has not promoted to its stable ABI
/// classification, so future kernels may evolve the rendered values. Parsing
/// therefore stays strict: recognized attributes with undocumented
/// renderings fail honestly instead of being guessed.
constexpr const char* dmi_id_root = "/sys/class/dmi/id/";

/// Trims the whitespace that one sysfs attribute read carries around its
/// value.
inline std::string_view trim_attribute(std::string_view value) noexcept {
    const auto blank = [](char letter) noexcept {
        return letter == ' ' || letter == '\t' || letter == '\r' ||
               letter == '\n';
    };
    while (!value.empty() && blank(value.front())) { value.remove_prefix(1U); }
    while (!value.empty() && blank(value.back())) { value.remove_suffix(1U); }
    return value;
}

/// Reports whether the running kernel exposes the DMI-id interface at all.
///
/// Machines whose firmware provides no DMI records, including many embedded
/// boards and some virtual machines, create no such directory. Every query
/// then reports not_supported so an unusable source can never masquerade as
/// recorded facts.
inline bool dmi_interface_present() noexcept {
    struct ::stat info;
    if (::stat(dmi_id_root, &info) != 0) { return false; }
    return S_ISDIR(info.st_mode);
}

/// Reads one DMI-id attribute and reduces it to a plain UTF-8 candidate.
///
/// A missing attribute file means that this platform records no such fact,
/// which is not_found rather than an error sentinel in a string. Every other
/// native failure propagates unchanged, so restricted permissions on
/// privileged attributes stay visible as the platform's own permission
/// error. A wholly blank rendering also records absence because firmware
/// strings can consist of padding alone and presenting emptiness would
/// present nothing as data.
inline result<std::string> read_attribute(const char* attribute) {
    const std::string path = std::string(dmi_id_root) + attribute;
    result<std::string> content =
        linux_platform::read_text_file(path.c_str());
    if (!content) {
        if (content.error() ==
            std::error_code(ENOENT, std::generic_category())) {
            return fail(errc::not_found);
        }
        return content;
    }
    const std::string_view trimmed =
        trim_attribute(std::string_view(*content));
    if (trimmed.empty()) { return fail(errc::not_found); }
    return std::string(trimmed);
}

/// Parses one strict nonnegative decimal rendering shared by numeric
/// attributes.
///
/// Sysfs renders numbers without signs or suffixes, so any other shape is
/// malformed platform data. The surrounding whitespace that one attribute
/// read carries is trimmed first.
inline result<std::uint32_t> parse_number(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value.empty()) { return fail(errc::malformed_data); }
    std::uint32_t parsed = 0U;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result outcome =
        std::from_chars(first, last, parsed);
    if (outcome.ec != std::errc() || outcome.ptr != last) {
        return fail(errc::malformed_data);
    }
    return parsed;
}

inline result<std::string> system_manufacturer() {
    if (!dmi_interface_present()) { return fail(errc::not_supported); }
    return read_attribute("sys_vendor");
}

inline result<std::string> system_product_name() {
    if (!dmi_interface_present()) { return fail(errc::not_supported); }
    return read_attribute("product_name");
}

inline result<std::string> system_product_version() {
    if (!dmi_interface_present()) { return fail(errc::not_supported); }
    return read_attribute("product_version");
}

inline result<std::string> motherboard_manufacturer() {
    if (!dmi_interface_present()) { return fail(errc::not_supported); }
    return read_attribute("board_vendor");
}

inline result<std::string> motherboard_product_name() {
    if (!dmi_interface_present()) { return fail(errc::not_supported); }
    return read_attribute("board_name");
}

inline result<std::string> motherboard_version() {
    if (!dmi_interface_present()) { return fail(errc::not_supported); }
    return read_attribute("board_version");
}

inline result<std::string> firmware_vendor() {
    if (!dmi_interface_present()) { return fail(errc::not_supported); }
    return read_attribute("bios_vendor");
}

inline result<std::string> firmware_version() {
    if (!dmi_interface_present()) { return fail(errc::not_supported); }
    return read_attribute("bios_version");
}

inline result<std::string> firmware_release_date() {
    if (!dmi_interface_present()) { return fail(errc::not_supported); }
    return read_attribute("bios_date");
}

inline result<hardware_common::chassis_classification> chassis_form_factor() {
    if (!dmi_interface_present()) { return fail(errc::not_supported); }
    const result<std::string> rendered = read_attribute("chassis_type");
    if (!rendered) { return fail(rendered.error()); }
    const result<std::uint32_t> recorded = parse_number(
        std::string_view(*rendered));
    if (!recorded) { return fail(recorded.error()); }
    if (*recorded > 255U) { return fail(errc::malformed_data); }
    return hardware_common::classify_chassis(
        static_cast<std::uint8_t>(*recorded));
}

inline result<std::string> hardware_uuid() {
    if (!dmi_interface_present()) { return fail(errc::not_supported); }
    // The public boundary validates the hyphenated rendering and reports
    // both SMBIOS-documented absence markers as not_found. The underlying
    // attribute file is readable by the privileged account only, so
    // unprivileged callers receive the native permission failure unchanged.
    return read_attribute("product_uuid");
}

} // namespace hardware_backend
} // namespace detail
} // namespace syscape

#endif
