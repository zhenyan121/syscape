#ifndef SYSCAPE_DETAIL_HARDWARE_FREEBSD_HPP
#define SYSCAPE_DETAIL_HARDWARE_FREEBSD_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <kenv.h>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/pciio.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/hardware/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_backend {

inline result<std::string> read_sysctl_string(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return fail(errc::not_found);
    }
    std::string value(size, '\0');
    if (::sysctlbyname(name, &value[0], &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n' ||
                              value.back() == '\r')) {
        value.pop_back();
    }
    if (value.empty()) {
        return fail(errc::not_found);
    }
    return value;
}

inline result<std::string> read_kenv_value(const char* name) {
    char buffer[256] = {};
    const int ret = ::kenv(KENV_GET, name, buffer, sizeof(buffer));
    if (ret < 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    std::size_t len = static_cast<std::size_t>(ret > 0 ? ret : 0);
    while (len > 0 && (buffer[len - 1] == '\0' || buffer[len - 1] == '\n' ||
                       buffer[len - 1] == '\r' || buffer[len - 1] == ' ')) {
        --len;
    }
    if (len == 0U) {
        return fail(errc::not_found);
    }
    return std::string(buffer, len);
}

inline result<std::string> system_manufacturer() {
    return read_kenv_value("smbios.system.maker");
}

inline result<std::string> system_product_name() {
    const result<std::string> smbios_prod =
        read_kenv_value("smbios.system.product");
    if (smbios_prod) {
        return smbios_prod;
    }
    return read_sysctl_string("hw.model");
}

inline result<std::string> system_product_version() {
    return read_kenv_value("smbios.system.version");
}

inline result<std::string> motherboard_manufacturer() {
    return read_kenv_value("smbios.planar.maker");
}

inline result<std::string> motherboard_product_name() {
    return read_kenv_value("smbios.planar.product");
}

inline result<std::string> motherboard_version() {
    return read_kenv_value("smbios.planar.version");
}

inline result<std::string> firmware_vendor() {
    return read_kenv_value("smbios.bios.vendor");
}

inline result<std::string> firmware_version() {
    return read_kenv_value("smbios.bios.version");
}

inline result<std::string> firmware_release_date() {
    return read_kenv_value("smbios.bios.reldate");
}

inline result<hardware_common::chassis_classification> chassis_form_factor() {
    const result<std::string> chassis_str =
        read_kenv_value("smbios.chassis.type");
    if (!chassis_str) {
        return fail(chassis_str.error());
    }
    std::uint32_t val = 0U;
    for (char c : *chassis_str) {
        if (c < '0' || c > '9') {
            return fail(errc::malformed_data);
        }
        val = val * 10U + static_cast<std::uint32_t>(c - '0');
        if (val > 255U) {
            return fail(errc::malformed_data);
        }
    }
    return hardware_common::classify_chassis(static_cast<std::uint8_t>(val));
}

inline result<std::string> hardware_uuid() {
    const result<std::string> smbios_uuid =
        read_kenv_value("smbios.system.uuid");
    if (smbios_uuid) {
        return smbios_uuid;
    }
    return read_sysctl_string("kern.hostuuid");
}

inline result<std::vector<::syscape::hardware::pci_device>> pci_devices() {
    const int fd = ::open("/dev/pci", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT || errno == ENODEV) {
            return std::vector<::syscape::hardware::pci_device>();
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    struct fd_guard {
        int handle;
        ~fd_guard() {
            if (handle >= 0) {
                ::close(handle);
            }
        }
    } guard {fd};

    std::vector<::syscape::hardware::pci_device> devices;
    struct pci_conf conf_matches[64] = {};
    struct pci_conf_io pc = {};
    pc.match_buf_len = sizeof(conf_matches);
    pc.matches = conf_matches;

    do {
        if (::ioctl(fd, PCIOCGETCONF, &pc) < 0) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (pc.status == PCI_GETCONF_ERROR) {
            return fail(errc::io_error);
        }

        for (unsigned int i = 0U; i < pc.num_matches; ++i) {
            const auto& match = conf_matches[i];
            ::syscape::hardware::pci_device dev;
            dev.domain = static_cast<std::uint16_t>(match.pc_sel.pc_domain);
            dev.bus = static_cast<std::uint8_t>(match.pc_sel.pc_bus);
            dev.device = static_cast<std::uint8_t>(match.pc_sel.pc_dev);
            dev.function = static_cast<std::uint8_t>(match.pc_sel.pc_func);
            dev.vendor_id = match.pc_vendor;
            dev.device_id = match.pc_device;
            dev.subsystem_vendor_id = match.pc_subvendor;
            dev.subsystem_device_id = match.pc_subdevice;
            dev.class_code = match.pc_class;
            dev.subclass_code = match.pc_subclass;
            dev.programming_interface = match.pc_progif;
            dev.device_class =
                hardware_common::classify_pci_class(match.pc_class);

            if (match.pd_name[0] != '\0') {
                std::string drv = match.pd_name;
                drv += std::to_string(match.pd_unit);
                dev.driver = std::move(drv);
            }
            devices.push_back(std::move(dev));
        }
    } while (pc.status == PCI_GETCONF_MORE_DEVS);

    std::sort(devices.begin(), devices.end(),
              hardware_common::compare_pci_devices);
    return devices;
}

inline result<std::vector<::syscape::hardware::usb_device>> usb_devices() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::hardware::memory_device>>
memory_devices() {
    return fail(errc::not_supported);
}

} // namespace hardware_backend
} // namespace detail
} // namespace syscape

#endif
