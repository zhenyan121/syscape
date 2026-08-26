#ifndef SYSCAPE_DETAIL_HARDWARE_LINUX_HPP
#define SYSCAPE_DETAIL_HARDWARE_LINUX_HPP

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <dirent.h>

#include <syscape/detail/hardware/common.hpp>
#include <syscape/detail/linux/directory.hpp>
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
template <typename Stat>
inline result<bool> dmi_interface_present_with(const Stat& stat_call) {
    struct ::stat info;
    if (stat_call(dmi_id_root, &info) != 0) {
        const int saved_errno = errno;
        if (saved_errno == ENOENT || saved_errno == ENOTDIR) { return false; }
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
    return S_ISDIR(info.st_mode);
}

inline result<bool> dmi_interface_present() {
    const auto stat_call = [](const char* path, struct ::stat* info) {
        return ::stat(path, info);
    };
    return dmi_interface_present_with(stat_call);
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

/// Verifies the shared DMI-id source before reading one attribute from it.
inline result<std::string> read_dmi_attribute(const char* attribute) {
    const result<bool> present = dmi_interface_present();
    if (!present) { return fail(present.error()); }
    if (!*present) { return fail(errc::not_supported); }
    return read_attribute(attribute);
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
    return read_dmi_attribute("sys_vendor");
}

inline result<std::string> system_product_name() {
    return read_dmi_attribute("product_name");
}

inline result<std::string> system_product_version() {
    return read_dmi_attribute("product_version");
}

inline result<std::string> motherboard_manufacturer() {
    return read_dmi_attribute("board_vendor");
}

inline result<std::string> motherboard_product_name() {
    return read_dmi_attribute("board_name");
}

inline result<std::string> motherboard_version() {
    return read_dmi_attribute("board_version");
}

inline result<std::string> firmware_vendor() {
    return read_dmi_attribute("bios_vendor");
}

inline result<std::string> firmware_version() {
    return read_dmi_attribute("bios_version");
}

inline result<std::string> firmware_release_date() {
    return read_dmi_attribute("bios_date");
}

inline result<hardware_common::chassis_classification> chassis_form_factor() {
    const result<std::string> rendered = read_dmi_attribute("chassis_type");
    if (!rendered) { return fail(rendered.error()); }
    const result<std::uint32_t> recorded = parse_number(
        std::string_view(*rendered));
    if (!recorded) { return fail(recorded.error()); }
    if (*recorded > 255U) { return fail(errc::malformed_data); }
    return hardware_common::classify_chassis(
        static_cast<std::uint8_t>(*recorded));
}

inline result<std::string> hardware_uuid() {
    // The public boundary validates the hyphenated rendering and reports
    // both SMBIOS-documented absence markers as not_found. The underlying
    // attribute file is readable by the privileged account only, so
    // unprivileged callers receive the native permission failure unchanged.
    return read_dmi_attribute("product_uuid");
}

/// Parses one hexadecimal rendering shared by sysfs device attributes.
inline result<std::uint32_t> parse_hex_u32(std::string_view input) {
    std::string_view value = trim_attribute(input);
    if (value.size() >= 2U && (value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))) {
        value.remove_prefix(2U);
    }
    if (value.empty()) { return fail(errc::malformed_data); }
    std::uint32_t parsed = 0U;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result outcome =
        std::from_chars(first, last, parsed, 16);
    if (outcome.ec != std::errc() || outcome.ptr != last) {
        return fail(errc::malformed_data);
    }
    return parsed;
}

/// Parses one decimal USB signaling speed, preserving an unknown link state.
inline result<std::optional<double>> parse_usb_speed_mbps(
    std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value.empty()) { return fail(errc::malformed_data); }
    if (value == "unknown") { return std::optional<double>(); }
    std::uint64_t whole = 0U;
    std::uint64_t fraction = 0U;
    std::uint64_t divisor = 1U;
    bool saw_dot = false;
    bool saw_digit = false;
    for (const char character : value) {
        if (character == '.' && !saw_dot) {
            saw_dot = true;
            continue;
        }
        if (character < '0' || character > '9') {
            return fail(errc::malformed_data);
        }
        saw_digit = true;
        const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
        if (!saw_dot) {
            if (whole > (UINT64_MAX - digit) / 10U) {
                return fail(errc::value_too_large);
            }
            whole = whole * 10U + digit;
        } else {
            if (divisor > UINT64_MAX / 10U ||
                fraction > (UINT64_MAX - digit) / 10U) {
                return fail(errc::value_too_large);
            }
            fraction = fraction * 10U + digit;
            divisor *= 10U;
        }
    }
    if (!saw_digit || (saw_dot && divisor == 1U)) {
        return fail(errc::malformed_data);
    }
    return std::optional<double>(
        static_cast<double>(whole) +
        static_cast<double>(fraction) / static_cast<double>(divisor));
}

/// Parses the last component of a USB devpath as the immediate upstream port.
inline result<std::optional<std::uint8_t>> parse_usb_port(
    std::string_view input) {
    std::string_view value = trim_attribute(input);
    if (value.empty()) { return fail(errc::malformed_data); }
    const std::size_t dot = value.rfind('.');
    if (dot != std::string_view::npos) { value.remove_prefix(dot + 1U); }
    const result<std::uint32_t> parsed = parse_number(value);
    if (!parsed) { return fail(parsed.error()); }
    if (*parsed == 0U) { return std::optional<std::uint8_t>(); }
    if (*parsed > 255U) { return fail(errc::malformed_data); }
    return std::optional<std::uint8_t>(static_cast<std::uint8_t>(*parsed));
}

inline bool device_attribute_disappeared(const std::error_code& error) noexcept {
    return error == std::error_code(ENOENT, std::generic_category()) ||
        error == std::error_code(ENODEV, std::generic_category());
}

/// Parses a PCI BDF (Bus/Device/Function) string like "0000:01:00.0".
inline bool parse_pci_bdf(std::string_view name,
                          std::uint16_t& domain,
                          std::uint8_t& bus,
                          std::uint8_t& device,
                          std::uint8_t& function) noexcept {
    const std::size_t first_colon = name.find(':');
    if (first_colon == std::string_view::npos || first_colon == 0U) { return false; }
    const std::size_t second_colon = name.find(':', first_colon + 1U);
    if (second_colon == std::string_view::npos) { return false; }
    const std::size_t dot = name.find('.', second_colon + 1U);
    if (dot == std::string_view::npos) { return false; }

    const std::string_view domain_str = name.substr(0U, first_colon);
    const std::string_view bus_str = name.substr(first_colon + 1U, second_colon - first_colon - 1U);
    const std::string_view dev_str = name.substr(second_colon + 1U, dot - second_colon - 1U);
    const std::string_view fn_str = name.substr(dot + 1U);

    if (domain_str.empty() || bus_str.empty() || dev_str.empty() || fn_str.empty()) {
        return false;
    }

    std::uint32_t dom = 0;
    std::uint32_t b = 0;
    std::uint32_t d = 0;
    std::uint32_t f = 0;

    auto res1 = std::from_chars(domain_str.data(), domain_str.data() + domain_str.size(), dom, 16);
    if (res1.ec != std::errc() || res1.ptr != domain_str.data() + domain_str.size() || dom > 0xFFFFU) {
        return false;
    }
    auto res2 = std::from_chars(bus_str.data(), bus_str.data() + bus_str.size(), b, 16);
    if (res2.ec != std::errc() || res2.ptr != bus_str.data() + bus_str.size() || b > 0xFFU) {
        return false;
    }
    auto res3 = std::from_chars(dev_str.data(), dev_str.data() + dev_str.size(), d, 16);
    if (res3.ec != std::errc() || res3.ptr != dev_str.data() + dev_str.size() || d > 0x1FU) {
        return false;
    }
    auto res4 = std::from_chars(fn_str.data(), fn_str.data() + fn_str.size(), f, 16);
    if (res4.ec != std::errc() || res4.ptr != fn_str.data() + fn_str.size() || f > 0x07U) {
        return false;
    }

    domain = static_cast<std::uint16_t>(dom);
    bus = static_cast<std::uint8_t>(b);
    device = static_cast<std::uint8_t>(d);
    function = static_cast<std::uint8_t>(f);
    return true;
}

inline result<std::vector<::syscape::hardware::pci_device>> pci_devices() {
    static constexpr const char* pci_dir_path = "/sys/bus/pci/devices";
    linux_platform::directory_handle dir(pci_dir_path);
    if (!dir.valid()) {
        if (dir.error() == ENOENT || dir.error() == ENOTDIR) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    std::vector<::syscape::hardware::pci_device> result_devices;
    for (;;) {
        errno = 0;
        struct ::dirent* const entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (entry->d_name[0] == '.') { continue; }
        const std::string name(entry->d_name);
        std::uint16_t domain = 0;
        std::uint8_t bus = 0;
        std::uint8_t device = 0;
        std::uint8_t function = 0;
        if (!parse_pci_bdf(name, domain, bus, device, function)) {
            continue;
        }

        const std::string dev_path = std::string(pci_dir_path) + "/" + name;
        ::syscape::hardware::pci_device dev;
        dev.domain = domain;
        dev.bus = bus;
        dev.device = device;
        dev.function = function;
        // Vendor
        const auto vendor_content = linux_platform::read_text_file((dev_path + "/vendor").c_str());
        if (!vendor_content) {
            if (device_attribute_disappeared(vendor_content.error())) { continue; }
            return fail(vendor_content.error());
        }
        const auto vendor_val = parse_hex_u32(*vendor_content);
        if (!vendor_val) { return fail(vendor_val.error()); }
        if (*vendor_val > 0xFFFFU) { return fail(errc::malformed_data); }
        dev.vendor_id = static_cast<std::uint16_t>(*vendor_val);

        // Device
        const auto device_content = linux_platform::read_text_file((dev_path + "/device").c_str());
        if (!device_content) {
            if (device_attribute_disappeared(device_content.error())) { continue; }
            return fail(device_content.error());
        }
        const auto device_val = parse_hex_u32(*device_content);
        if (!device_val) { return fail(device_val.error()); }
        if (*device_val > 0xFFFFU) { return fail(errc::malformed_data); }
        dev.device_id = static_cast<std::uint16_t>(*device_val);

        // Class
        const auto class_content = linux_platform::read_text_file((dev_path + "/class").c_str());
        if (!class_content) {
            if (device_attribute_disappeared(class_content.error())) { continue; }
            return fail(class_content.error());
        }
        const auto class_val = parse_hex_u32(*class_content);
        if (!class_val) { return fail(class_val.error()); }
        if (*class_val > 0xFFFFFFU) { return fail(errc::malformed_data); }
        const std::uint8_t base_class =
            static_cast<std::uint8_t>((*class_val >> 16U) & 0xFFU);
        dev.class_code = base_class;
        dev.subclass_code = static_cast<std::uint8_t>((*class_val >> 8U) & 0xFFU);
        dev.programming_interface = static_cast<std::uint8_t>(*class_val & 0xFFU);
        dev.device_class = hardware_common::classify_pci_class(base_class);

        // Subsystem Vendor (optional)
        const auto subsys_vendor_content = linux_platform::read_text_file((dev_path + "/subsystem_vendor").c_str());
        if (subsys_vendor_content) {
            const auto subsys_vendor_val = parse_hex_u32(*subsys_vendor_content);
            if (!subsys_vendor_val || *subsys_vendor_val > 0xFFFFU) {
                return fail(errc::malformed_data);
            }
            dev.subsystem_vendor_id = static_cast<std::uint16_t>(*subsys_vendor_val);
        } else if (!device_attribute_disappeared(subsys_vendor_content.error())) {
            return fail(subsys_vendor_content.error());
        }

        // Subsystem Device (optional)
        const auto subsys_device_content = linux_platform::read_text_file((dev_path + "/subsystem_device").c_str());
        if (subsys_device_content) {
            const auto subsys_device_val = parse_hex_u32(*subsys_device_content);
            if (!subsys_device_val || *subsys_device_val > 0xFFFFU) {
                return fail(errc::malformed_data);
            }
            dev.subsystem_device_id = static_cast<std::uint16_t>(*subsys_device_val);
        } else if (!device_attribute_disappeared(subsys_device_content.error())) {
            return fail(subsys_device_content.error());
        }

        // Driver (symlink target)
        char driver_buf[1024];
        errno = 0;
        const ssize_t link_len = ::readlink((dev_path + "/driver").c_str(), driver_buf, sizeof(driver_buf) - 1U);
        if (link_len > 0) {
            if (static_cast<std::size_t>(link_len) == sizeof(driver_buf) - 1U) {
                return fail(errc::value_too_large);
            }
            driver_buf[link_len] = '\0';
            std::string_view link_view(driver_buf, static_cast<std::size_t>(link_len));
            const std::size_t last_slash = link_view.rfind('/');
            if (last_slash != std::string_view::npos) {
                link_view.remove_prefix(last_slash + 1U);
            }
            if (!link_view.empty()) {
                dev.driver = std::string(link_view);
            }
        } else if (link_len < 0 && errno != ENOENT && errno != ENODEV && errno != EINVAL) {
            return fail(std::error_code(errno, std::generic_category()));
        }

        result_devices.push_back(std::move(dev));
    }

    std::sort(result_devices.begin(), result_devices.end(), hardware_common::compare_pci_devices);
    return result_devices;
}

inline result<std::vector<::syscape::hardware::usb_device>> usb_devices() {
    static constexpr const char* usb_dir_path = "/sys/bus/usb/devices";
    linux_platform::directory_handle dir(usb_dir_path);
    if (!dir.valid()) {
        if (dir.error() == ENOENT || dir.error() == ENOTDIR) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    std::vector<::syscape::hardware::usb_device> result_devices;
    for (;;) {
        errno = 0;
        struct ::dirent* const entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (entry->d_name[0] == '.') { continue; }
        const std::string name(entry->d_name);
        // Skip interface subdirectories (which contain ':')
        if (name.find(':') != std::string::npos) { continue; }

        const std::string dev_path = std::string(usb_dir_path) + "/" + name;

        // idVendor is mandatory to qualify as a USB device node
        const auto vendor_content = linux_platform::read_text_file((dev_path + "/idVendor").c_str());
        if (!vendor_content) {
            if (device_attribute_disappeared(vendor_content.error())) { continue; }
            return fail(vendor_content.error());
        }
        const auto vendor_val = parse_hex_u32(*vendor_content);
        if (!vendor_val || *vendor_val > 0xFFFFU) {
            return fail(errc::malformed_data);
        }

        // idProduct
        const auto product_content = linux_platform::read_text_file((dev_path + "/idProduct").c_str());
        if (!product_content) {
            if (device_attribute_disappeared(product_content.error())) { continue; }
            return fail(product_content.error());
        }
        const auto product_val = parse_hex_u32(*product_content);
        if (!product_val || *product_val > 0xFFFFU) {
            return fail(errc::malformed_data);
        }

        ::syscape::hardware::usb_device dev;
        dev.vendor_id = static_cast<std::uint16_t>(*vendor_val);
        dev.product_id = static_cast<std::uint16_t>(*product_val);

        // busnum
        const auto bus_content = linux_platform::read_text_file((dev_path + "/busnum").c_str());
        if (bus_content) {
            const auto bus_val = parse_number(*bus_content);
            if (!bus_val || *bus_val > 255U) { return fail(errc::malformed_data); }
            dev.bus_number = static_cast<std::uint8_t>(*bus_val);
        } else if (!device_attribute_disappeared(bus_content.error())) {
            return fail(bus_content.error());
        }

        // devnum
        const auto dev_content = linux_platform::read_text_file((dev_path + "/devnum").c_str());
        if (dev_content) {
            const auto dev_val = parse_number(*dev_content);
            if (!dev_val || *dev_val > 127U) { return fail(errc::malformed_data); }
            dev.device_address = static_cast<std::uint8_t>(*dev_val);
        } else if (!device_attribute_disappeared(dev_content.error())) {
            return fail(dev_content.error());
        }

        // devpath / port
        const auto devpath_content = linux_platform::read_text_file((dev_path + "/devpath").c_str());
        if (devpath_content) {
            const auto port_val = parse_usb_port(*devpath_content);
            if (!port_val) { return fail(port_val.error()); }
            dev.port_number = *port_val;
        } else if (!device_attribute_disappeared(devpath_content.error())) {
            return fail(devpath_content.error());
        }

        // bcdDevice
        const auto bcd_content = linux_platform::read_text_file((dev_path + "/bcdDevice").c_str());
        if (bcd_content) {
            const auto bcd_val = parse_hex_u32(*bcd_content);
            if (!bcd_val || *bcd_val > 0xFFFFU) { return fail(errc::malformed_data); }
            dev.bcd_device = static_cast<std::uint16_t>(*bcd_val);
        } else if (!device_attribute_disappeared(bcd_content.error())) {
            return fail(bcd_content.error());
        }

        // bDeviceClass
        const auto class_content = linux_platform::read_text_file((dev_path + "/bDeviceClass").c_str());
        if (class_content) {
            const auto class_val = parse_hex_u32(*class_content);
            if (!class_val || *class_val > 0xFFU) { return fail(errc::malformed_data); }
            dev.device_class = static_cast<std::uint8_t>(*class_val);
        } else if (!device_attribute_disappeared(class_content.error())) {
            return fail(class_content.error());
        }

        // bDeviceSubClass
        const auto subclass_content = linux_platform::read_text_file((dev_path + "/bDeviceSubClass").c_str());
        if (subclass_content) {
            const auto subclass_val = parse_hex_u32(*subclass_content);
            if (!subclass_val || *subclass_val > 0xFFU) { return fail(errc::malformed_data); }
            dev.device_subclass = static_cast<std::uint8_t>(*subclass_val);
        } else if (!device_attribute_disappeared(subclass_content.error())) {
            return fail(subclass_content.error());
        }

        // bDeviceProtocol
        const auto proto_content = linux_platform::read_text_file((dev_path + "/bDeviceProtocol").c_str());
        if (proto_content) {
            const auto proto_val = parse_hex_u32(*proto_content);
            if (!proto_val || *proto_val > 0xFFU) { return fail(errc::malformed_data); }
            dev.device_protocol = static_cast<std::uint8_t>(*proto_val);
        } else if (!device_attribute_disappeared(proto_content.error())) {
            return fail(proto_content.error());
        }

        // manufacturer
        const auto mfg_content = linux_platform::read_text_file((dev_path + "/manufacturer").c_str());
        if (mfg_content) {
            const std::string_view trimmed = trim_attribute(*mfg_content);
            if (!trimmed.empty()) { dev.manufacturer = std::string(trimmed); }
        } else if (!device_attribute_disappeared(mfg_content.error())) {
            return fail(mfg_content.error());
        }

        // product
        const auto prod_content = linux_platform::read_text_file((dev_path + "/product").c_str());
        if (prod_content) {
            const std::string_view trimmed = trim_attribute(*prod_content);
            if (!trimmed.empty()) { dev.product = std::string(trimmed); }
        } else if (!device_attribute_disappeared(prod_content.error())) {
            return fail(prod_content.error());
        }

        // serial
        const auto ser_content = linux_platform::read_text_file((dev_path + "/serial").c_str());
        if (ser_content) {
            const std::string_view trimmed = trim_attribute(*ser_content);
            if (!trimmed.empty()) { dev.serial_number = std::string(trimmed); }
        } else if (!device_attribute_disappeared(ser_content.error())) {
            return fail(ser_content.error());
        }

        // speed
        const auto speed_content = linux_platform::read_text_file((dev_path + "/speed").c_str());
        if (speed_content) {
            const result<std::optional<double>> speed =
                parse_usb_speed_mbps(*speed_content);
            if (!speed) { return fail(speed.error()); }
            dev.speed_mbps = *speed;
        } else if (!device_attribute_disappeared(speed_content.error())) {
            return fail(speed_content.error());
        }

        result_devices.push_back(std::move(dev));
    }

    std::sort(result_devices.begin(), result_devices.end(), hardware_common::compare_usb_devices);
    return result_devices;
}

inline result<std::vector<::syscape::hardware::memory_device>> memory_devices() {
    static constexpr const char* dmi_table_path = "/sys/firmware/dmi/tables/DMI";
    const result<std::string> table = linux_platform::read_text_file(dmi_table_path, 4U * 1024U * 1024U);
    if (!table) {
        if (table.error() == std::error_code(ENOENT, std::generic_category()) ||
            table.error() == std::error_code(ENODEV, std::generic_category())) {
            return fail(errc::not_supported);
        }
        return fail(table.error());
    }
    return hardware_common::parse_smbios_memory_devices(
        reinterpret_cast<const std::uint8_t*>(table->data()), table->size());
}

} // namespace hardware_backend
} // namespace detail
} // namespace syscape

#endif
