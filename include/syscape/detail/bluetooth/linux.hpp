#ifndef SYSCAPE_DETAIL_BLUETOOTH_LINUX_HPP
#define SYSCAPE_DETAIL_BLUETOOTH_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/bluetooth.hpp>
#include <syscape/detail/bluetooth/common.hpp>
#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH 31
#endif

#ifndef BTPROTO_HCI
#define BTPROTO_HCI 1
#endif

#ifndef HCIGETDEVLIST
#define HCIGETDEVLIST _IOR('H', 210, int)
#endif

#ifndef HCIGETDEVINFO
#define HCIGETDEVINFO _IOR('H', 211, int)
#endif

#ifndef HCIGETCONNLIST
#define HCIGETCONNLIST _IOR('H', 212, int)
#endif

namespace syscape {
namespace detail {
namespace bluetooth_backend {

struct bdaddr_t {
    std::uint8_t b[6];
};

struct hci_dev_stats {
    std::uint32_t err_rx;
    std::uint32_t err_tx;
    std::uint32_t cmd_tx;
    std::uint32_t evt_rx;
    std::uint32_t acl_tx;
    std::uint32_t acl_rx;
    std::uint32_t sco_tx;
    std::uint32_t sco_rx;
    std::uint32_t byte_rx;
    std::uint32_t byte_tx;
};

struct hci_dev_info {
    std::uint16_t dev_id;
    char name[8];
    bdaddr_t bdaddr;
    std::uint32_t flags;
    std::uint8_t type;
    std::uint8_t features[8];
    std::uint32_t pkt_type;
    std::uint32_t link_policy;
    std::uint32_t link_mode;
    std::uint16_t acl_mtu;
    std::uint16_t acl_pkts;
    std::uint16_t sco_mtu;
    std::uint16_t sco_pkts;
    hci_dev_stats stat;
};

struct hci_conn_info {
    std::uint16_t handle;
    bdaddr_t bdaddr;
    std::uint8_t type;
    std::uint8_t out;
    std::uint16_t state;
    std::uint32_t link_mode;
};

struct hci_conn_list_req {
    std::uint16_t dev_id;
    std::uint16_t conn_num;
    hci_conn_info conn_info[64];
};

inline result<const ::dirent*> next_directory_entry(::DIR* directory) {
    errno = 0;
    const auto* entry = ::readdir(directory);
    if (entry == nullptr && errno != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return entry;
}

inline result<std::optional<std::string>>
read_sysfs_string(const std::string& path) {
    auto content = linux_platform::read_text_file(path.c_str(), 4096U);
    if (!content) {
        if (content.error() == std::errc::no_such_file_or_directory ||
            content.error() == std::errc::not_a_directory) {
            return std::optional<std::string>{};
        }
        return fail(content.error());
    }
    linux_platform::trim_line_end(*content);
    std::string_view trimmed = bluetooth_common::trim_whitespace(*content);
    if (trimmed.empty()) {
        return std::optional<std::string>{};
    }
    if (!is_valid_utf8(trimmed)) {
        return fail(errc::invalid_encoding);
    }
    return std::optional<std::string>{std::string(trimmed)};
}

template <typename IntType>
inline result<std::optional<IntType>>
read_sysfs_int(const std::string& path, int base = 10) {
    auto str_res = read_sysfs_string(path);
    if (!str_res) {
        return fail(str_res.error());
    }
    if (!str_res->has_value()) {
        return std::optional<IntType>{};
    }
    auto val = bluetooth_common::parse_int<IntType>(**str_res, base);
    if (!val) {
        return fail(errc::malformed_data);
    }
    return val;
}

inline bluetooth::adapter_bus_type
detect_bus_type(const std::string& adapter_dir) {
    char target_buf[1024];
    std::string subsystem_link = adapter_dir + "/device/subsystem";
    ssize_t len =
        ::readlink(subsystem_link.c_str(), target_buf, sizeof(target_buf) - 1);
    if (len > 0) {
        target_buf[len] = '\0';
        std::string_view target(target_buf, static_cast<std::size_t>(len));
        if (target.find("usb") != std::string_view::npos) {
            return bluetooth::adapter_bus_type::usb;
        }
        if (target.find("pci") != std::string_view::npos) {
            return bluetooth::adapter_bus_type::pci;
        }
        if (target.find("sdio") != std::string_view::npos) {
            return bluetooth::adapter_bus_type::sdio;
        }
        if (target.find("serial") != std::string_view::npos ||
            target.find("tty") != std::string_view::npos ||
            target.find("uart") != std::string_view::npos) {
            return bluetooth::adapter_bus_type::uart;
        }
        if (target.find("platform") != std::string_view::npos ||
            target.find("amba") != std::string_view::npos) {
            return bluetooth::adapter_bus_type::built_in;
        }
        if (target.find("virtio") != std::string_view::npos ||
            target.find("vhci") != std::string_view::npos) {
            return bluetooth::adapter_bus_type::virtual_bus;
        }
    }
    return bluetooth::adapter_bus_type::unknown;
}

inline result<bluetooth::adapter_power_state>
detect_rfkill_state(const std::string& adapter_id,
                    const std::string& adapter_dir,
                    const std::string& rfkill_root = "/sys/class/rfkill") {
    // First check rfkill subfolder in adapter_dir (e.g. /sys/class/bluetooth/hci0/rfkill0)
    linux_platform::directory_handle dir(adapter_dir.c_str());
    if (dir.valid()) {
        for (;;) {
            auto entry_result = next_directory_entry(dir.get());
            if (!entry_result) {
                return fail(entry_result.error());
            }
            const auto* entry = *entry_result;
            if (entry == nullptr) {
                break;
            }
            if (std::strncmp(entry->d_name, "rfkill", 6) == 0) {
                std::string rf_path = adapter_dir + "/" + entry->d_name;
                auto soft_res = read_sysfs_int<int>(rf_path + "/soft");
                auto hard_res = read_sysfs_int<int>(rf_path + "/hard");
                if (!soft_res) {
                    return fail(soft_res.error());
                }
                if (!hard_res) {
                    return fail(hard_res.error());
                }
                if (soft_res && hard_res && (soft_res->has_value() || hard_res->has_value())) {
                    const int soft = soft_res->value_or(0);
                    const int hard = hard_res->value_or(0);
                    if (soft == 1 || hard == 1) {
                        return bluetooth::adapter_power_state::blocked;
                    }
                }
            }
        }
    }

    // Next check system-wide /sys/class/rfkill/
    linux_platform::directory_handle sys_rfkill(rfkill_root.c_str());
    if (sys_rfkill.valid()) {
        for (;;) {
            auto entry_result = next_directory_entry(sys_rfkill.get());
            if (!entry_result) {
                return fail(entry_result.error());
            }
            const auto* entry = *entry_result;
            if (entry == nullptr) {
                break;
            }
            if (entry->d_name[0] == '.') {
                continue;
            }
            std::string rf_path = rfkill_root + "/" + entry->d_name;
            auto name_res = read_sysfs_string(rf_path + "/name");
            auto type_res = read_sysfs_string(rf_path + "/type");
            if (!name_res) {
                return fail(name_res.error());
            }
            if (!type_res) {
                return fail(type_res.error());
            }
            const bool matches = name_res->has_value() &&
                                 **name_res == adapter_id &&
                                 type_res->has_value() &&
                                 **type_res == "bluetooth";
            if (matches) {
                auto soft_res = read_sysfs_int<int>(rf_path + "/soft");
                auto hard_res = read_sysfs_int<int>(rf_path + "/hard");
                if (!soft_res) {
                    return fail(soft_res.error());
                }
                if (!hard_res) {
                    return fail(hard_res.error());
                }
                if (soft_res && hard_res && (soft_res->has_value() || hard_res->has_value())) {
                    const int soft = soft_res->value_or(0);
                    const int hard = hard_res->value_or(0);
                    if (soft == 1 || hard == 1) {
                        return bluetooth::adapter_power_state::blocked;
                    }
                }
            }
        }
    }

    return bluetooth::adapter_power_state::unknown;
}

inline void apply_hci_flags(std::uint32_t flags,
                            bluetooth::adapter_info& info) noexcept {
    constexpr std::uint32_t hci_up = 1U << 0U;
    constexpr std::uint32_t hci_page_scan = 1U << 3U;
    constexpr std::uint32_t hci_inquiry_scan = 1U << 4U;

    if (info.power_state != bluetooth::adapter_power_state::blocked) {
        info.power_state = (flags & hci_up) != 0U
                               ? bluetooth::adapter_power_state::on
                               : bluetooth::adapter_power_state::off;
    }
    info.is_connectable = (flags & hci_page_scan) != 0U;
    info.is_discoverable = (flags & hci_inquiry_scan) != 0U;
}

inline void query_hci_ioctl(int dev_id, bluetooth::adapter_info& info) {
    int sock = ::socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (sock < 0) {
        return;
    }
    const linux_platform::file_descriptor owned_sock(sock);
    static_cast<void>(owned_sock);

    hci_dev_info di{};
    di.dev_id = static_cast<std::uint16_t>(dev_id);
    if (::ioctl(sock, HCIGETDEVINFO, &di) == 0) {
        if (!info.address.has_value()) {
            info.address = bluetooth_common::format_mac_bytes(di.bdaddr.b, true);
        }
        if (info.name.empty() && di.name[0] != '\0') {
            std::string_view dname(di.name, ::strnlen(di.name, sizeof(di.name)));
            if (is_valid_utf8(dname)) {
                info.name = std::string(dname);
            }
        }
        apply_hci_flags(di.flags, info);
    }
}

inline result<std::vector<bluetooth::adapter_info>> adapters() {
    std::vector<bluetooth::adapter_info> result_list;
    const char* sys_bluetooth = "/sys/class/bluetooth";
    linux_platform::directory_handle dir(sys_bluetooth);

    if (!dir.valid()) {
        if (dir.error() == ENOENT) {
            // Bluetooth subsystem not active or no hardware present
            return result_list;
        }
        if (dir.error() == EACCES || dir.error() == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    std::vector<std::string> adapter_names;
    for (;;) {
        auto entry_result = next_directory_entry(dir.get());
        if (!entry_result) {
            return fail(entry_result.error());
        }
        const auto* entry = *entry_result;
        if (entry == nullptr) {
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (std::strncmp(entry->d_name, "hci", 3) == 0) {
            adapter_names.emplace_back(entry->d_name);
        }
    }

    std::sort(adapter_names.begin(), adapter_names.end(),
              bluetooth_common::natural_less);

    for (const auto& name : adapter_names) {
        const std::string adapter_dir = std::string(sys_bluetooth) + "/" + name;
        bluetooth::adapter_info info;
        info.id = name;

        // Name
        auto name_res = read_sysfs_string(adapter_dir + "/name");
        if (!name_res) {
            return fail(name_res.error());
        }
        if (name_res && name_res->has_value()) {
            info.name = **name_res;
        }

        // MAC Address
        auto addr_res = read_sysfs_string(adapter_dir + "/address");
        if (!addr_res) {
            return fail(addr_res.error());
        }
        if (addr_res && addr_res->has_value()) {
            auto norm = bluetooth_common::normalize_mac_address(**addr_res);
            if (!norm) {
                return fail(errc::malformed_data);
            }
            info.address = *norm;
        }

        // rfkill determines whether the adapter is blocked. The HCI_UP flag
        // queried below determines whether an unblocked controller is on.
        auto power_state = detect_rfkill_state(name, adapter_dir);
        if (!power_state) {
            return fail(power_state.error());
        }
        info.power_state = *power_state;

        // Bus type
        info.bus = detect_bus_type(adapter_dir);

        // Try the parent USB device if the controller name is still missing.
        if (info.bus == bluetooth::adapter_bus_type::usb) {
            std::string usb_dev_dir = adapter_dir + "/device/..";
            if (info.name.empty()) {
                auto prod_res = read_sysfs_string(usb_dev_dir + "/product");
                if (!prod_res) {
                    return fail(prod_res.error());
                }
                if (prod_res && prod_res->has_value()) {
                    info.name = **prod_res;
                }
            }
        }

        // Query HCI state flags and supplement missing identity fields.
        int dev_id = 0;
        if (name.size() > 3) {
            auto id_num = bluetooth_common::parse_int<int>(name.substr(3));
            if (id_num) {
                dev_id = *id_num;
            }
        }
        query_hci_ioctl(dev_id, info);

        if (info.name.empty()) {
            info.name = name;
        }

        result_list.push_back(std::move(info));
    }

    return result_list;
}

inline result<std::size_t> adapter_count() {
    auto res = adapters();
    if (!res) {
        return fail(res.error());
    }
    return res->size();
}

inline result<bluetooth::adapter_info> default_adapter() {
    auto res = adapters();
    if (!res) {
        return fail(res.error());
    }
    if (res->empty()) {
        return fail(errc::not_found);
    }
    // Find hci0 first
    for (const auto& ad : *res) {
        if (ad.id == "hci0") {
            return ad;
        }
    }
    // Otherwise return first adapter
    return (*res)[0];
}

inline result<std::vector<bluetooth::device_info>> connected_devices() {
    std::vector<bluetooth::device_info> result_list;
    auto ad_res = adapters();
    if (!ad_res) {
        return fail(ad_res.error());
    }
    if (ad_res->empty()) {
        return result_list;
    }

    int sock = ::socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (sock < 0) {
        const int error = errno;
        if (error == EACCES || error == EPERM) {
            return fail(errc::permission_denied);
        }
        if (error == EAFNOSUPPORT || error == EPROTONOSUPPORT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(error, std::generic_category()));
    }
    linux_platform::file_descriptor owned_socket(sock);
    static_cast<void>(owned_socket);

    for (const auto& ad : *ad_res) {
        if (ad.id.size() <= 3U) {
            return fail(errc::malformed_data);
        }
        auto parsed = bluetooth_common::parse_int<std::uint16_t>(ad.id.substr(3));
        if (!parsed) {
            return fail(errc::malformed_data);
        }
        hci_conn_list_req cl{};
        cl.dev_id = *parsed;
        cl.conn_num = 64;
        int ioctl_result = -1;
        do {
            ioctl_result = ::ioctl(sock, HCIGETCONNLIST, &cl);
        } while (ioctl_result < 0 && errno == EINTR);
        if (ioctl_result < 0) {
            const int error = errno;
            if (error == EACCES || error == EPERM) {
                return fail(errc::permission_denied);
            }
            if (error == EOPNOTSUPP || error == ENOTTY) {
                return fail(errc::not_supported);
            }
            return fail(std::error_code(error, std::generic_category()));
        }
        for (std::size_t i = 0; i < cl.conn_num && i < 64U; ++i) {
            bluetooth::device_info d;
            d.adapter_id = ad.id;
            d.address = bluetooth_common::format_mac_bytes(
                cl.conn_info[i].bdaddr.b, true);
            d.is_connected = true;
            result_list.push_back(std::move(d));
        }
    }
    return result_list;
}

inline result<std::vector<bluetooth::device_info>>
paired_devices(const char* bluez_root = "/var/lib/bluetooth") {
    std::vector<bluetooth::device_info> result_list;
    linux_platform::directory_handle root_dir(bluez_root);
    if (!root_dir.valid()) {
        if (root_dir.error() == ENOENT) {
            return result_list;
        }
        if (root_dir.error() == EACCES || root_dir.error() == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(root_dir.error(), std::generic_category()));
    }

    for (;;) {
        auto adapter_entry_result = next_directory_entry(root_dir.get());
        if (!adapter_entry_result) {
            return fail(adapter_entry_result.error());
        }
        const auto* ad_entry = *adapter_entry_result;
        if (ad_entry == nullptr) {
            break;
        }
        if (ad_entry->d_name[0] == '.') {
            continue;
        }
        auto ad_mac = bluetooth_common::normalize_mac_address(ad_entry->d_name);
        if (!ad_mac) {
            continue;
        }
        std::string adapter_path = std::string(bluez_root) + "/" + ad_entry->d_name;
        linux_platform::directory_handle ad_dir(adapter_path.c_str());
        if (!ad_dir.valid()) {
            if (ad_dir.error() == ENOENT) {
                continue;
            }
            if (ad_dir.error() == EACCES || ad_dir.error() == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(ad_dir.error(), std::generic_category()));
        }

        for (;;) {
            auto device_entry_result = next_directory_entry(ad_dir.get());
            if (!device_entry_result) {
                return fail(device_entry_result.error());
            }
            const auto* dev_entry = *device_entry_result;
            if (dev_entry == nullptr) {
                break;
            }
            if (dev_entry->d_name[0] == '.') {
                continue;
            }
            auto dev_mac = bluetooth_common::normalize_mac_address(dev_entry->d_name);
            if (!dev_mac) {
                continue;
            }

            bluetooth::device_info dev;
            dev.address = *dev_mac;
            dev.is_paired = true;

            // Parse /var/lib/bluetooth/[adapter_mac]/[dev_mac]/info
            std::string info_path = adapter_path + "/" + dev_entry->d_name + "/info";
            auto content_res = linux_platform::read_text_file(info_path.c_str(), 16384U);
            if (!content_res) {
                if (content_res.error() == std::errc::no_such_file_or_directory) {
                    continue;
                }
                if (content_res.error() == std::errc::permission_denied) {
                    return fail(errc::permission_denied);
                }
                return fail(content_res.error());
            } else {
                std::string_view content(*content_res);
                while (!content.empty()) {
                    auto nl = content.find('\n');
                    std::string_view line = (nl == std::string_view::npos) ? content : content.substr(0, nl);
                    content = (nl == std::string_view::npos) ? std::string_view{} : content.substr(nl + 1);

                    line = bluetooth_common::trim_whitespace(line);
                    if (line.empty() || line.front() == '[' || line.front() == '#') {
                        continue;
                    }
                    auto eq = line.find('=');
                    if (eq == std::string_view::npos) {
                        continue;
                    }
                    std::string_view key = bluetooth_common::trim_whitespace(line.substr(0, eq));
                    std::string_view val = bluetooth_common::trim_whitespace(line.substr(eq + 1));

                    if (key == "Name" || key == "Alias") {
                        if (!is_valid_utf8(val)) {
                            return fail(errc::invalid_encoding);
                        }
                        if (!dev.name.has_value()) {
                            dev.name = std::string(val);
                        }
                    } else if (key == "Class") {
                        auto cod = bluetooth_common::parse_int<std::uint32_t>(val, 16);
                        if (!cod) {
                            cod = bluetooth_common::parse_int<std::uint32_t>(val, 10);
                        }
                        if (!cod) {
                            return fail(errc::malformed_data);
                        }
                        dev.class_of_device = *cod;
                        dev.device_type = bluetooth_common::decode_major_device_class(*cod);
                    } else if (key == "Trusted") {
                        if (val != "true" && val != "false" && val != "1" && val != "0") {
                            return fail(errc::malformed_data);
                        }
                        dev.is_trusted = (val == "true" || val == "1");
                    } else if (key == "Blocked") {
                        if (val != "true" && val != "false" && val != "1" && val != "0") {
                            return fail(errc::malformed_data);
                        }
                        dev.is_blocked = (val == "true" || val == "1");
                    } else if (key == "Bonded") {
                        if (val != "true" && val != "false" && val != "1" && val != "0") {
                            return fail(errc::malformed_data);
                        }
                        dev.is_paired = (val == "true" || val == "1");
                    }
                }
            }

            if (dev.is_paired != false) {
                result_list.push_back(std::move(dev));
            }
        }
    }

    return result_list;
}

} // namespace bluetooth_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_BLUETOOTH_LINUX_HPP
