#ifndef SYSCAPE_DETAIL_CAMERA_LINUX_HPP
#define SYSCAPE_DETAIL_CAMERA_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/camera.hpp>
#include <syscape/detail/camera/common.hpp>
#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace camera_backend {

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
    std::string_view trimmed = camera_common::trim_whitespace(*content);
    if (trimmed.empty()) {
        return std::optional<std::string>{};
    }
    if (!is_valid_utf8(trimmed)) {
        return fail(errc::invalid_encoding);
    }
    return std::optional<std::string>{std::string(trimmed)};
}

inline result<std::optional<::syscape::camera::camera_device_id>>
parse_usb_ids(const std::string& usb_dev_dir) {
    const auto vendor_str = read_sysfs_string(usb_dev_dir + "/idVendor");
    const auto product_str = read_sysfs_string(usb_dev_dir + "/idProduct");
    if (!vendor_str || !product_str) {
        return fail(!vendor_str ? vendor_str.error() : product_str.error());
    }
    if (!vendor_str->has_value() && !product_str->has_value()) {
        return std::optional<::syscape::camera::camera_device_id>{};
    }
    if (!vendor_str->has_value() || !product_str->has_value()) {
        return fail(errc::malformed_data);
    }
    const auto vendor = camera_common::parse_hex_u16(**vendor_str);
    const auto product = camera_common::parse_hex_u16(**product_str);
    if (!vendor || !product) {
        return fail(!vendor ? vendor.error() : product.error());
    }
    ::syscape::camera::camera_device_id id;
    id.vendor_id = *vendor;
    id.product_id = *product;

    const auto bcd_str = read_sysfs_string(usb_dev_dir + "/bcdDevice");
    if (!bcd_str) {
        return fail(bcd_str.error());
    }
    if (bcd_str->has_value()) {
        const auto bcd = camera_common::parse_hex_u16(**bcd_str);
        if (!bcd) {
            return fail(bcd.error());
        }
        id.revision = *bcd;
    }
    return std::optional<::syscape::camera::camera_device_id>{id};
}

inline ::syscape::camera::camera_facing
detect_facing_from_panel(std::string_view panel_str) noexcept {
    if (camera_common::contains_ignore_case(panel_str, "top") ||
        camera_common::contains_ignore_case(panel_str, "front")) {
        return ::syscape::camera::camera_facing::front;
    }
    if (camera_common::contains_ignore_case(panel_str, "back") ||
        camera_common::contains_ignore_case(panel_str, "bottom")) {
        return ::syscape::camera::camera_facing::back;
    }
    if (camera_common::contains_ignore_case(panel_str, "external")) {
        return ::syscape::camera::camera_facing::external;
    }
    return ::syscape::camera::camera_facing::unknown;
}

inline result<std::optional<std::string>> v4l2_text(const unsigned char* data,
                                                    std::size_t size) {
    std::size_t length = 0U;
    while (length < size && data[length] != 0U) {
        ++length;
    }
    if (length == size) {
        return fail(errc::malformed_data);
    }
    if (length == 0U) {
        return std::optional<std::string>{};
    }
    const std::string value(reinterpret_cast<const char*>(data), length);
    if (!is_valid_utf8(value)) {
        return fail(errc::invalid_encoding);
    }
    return std::optional<std::string>{value};
}

inline result<std::vector<::syscape::camera::camera_device>>
enumerate_devices(bool require_capture_capability) {
    constexpr const char* base_dir = "/sys/class/video4linux";
    const linux_platform::directory_handle dir(base_dir);
    if (!dir.valid()) {
        if (dir.error() == ENOENT) {
            return std::vector<::syscape::camera::camera_device>{};
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    std::vector<::syscape::camera::camera_device> device_list;
    struct dirent* entry = nullptr;

    for (;;) {
        errno = 0;
        entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }
        const std::string id(entry->d_name);
        if (id.rfind("video", 0) != 0 && id.rfind("v4l-subdev", 0) != 0) {
            continue;
        }

        const std::string sysfs_entry = std::string(base_dir) + "/" + id;
        ::syscape::camera::camera_device dev;
        dev.id = id;
        dev.sysfs_path = sysfs_entry;
        dev.device_path = std::string("/dev/") + id;

        const auto name_str = read_sysfs_string(sysfs_entry + "/name");
        if (!name_str) {
            return fail(name_str.error());
        }
        if (name_str->has_value()) {
            dev.name = **name_str;
        } else {
            dev.name = id;
        }

        if (id.rfind("v4l-subdev", 0) == 0) {
            ::syscape::camera::camera_capabilities caps;
            caps.has_video_capture = false;
            dev.capabilities = caps;
        } else {
            // Query capabilities without starting a capture stream.
            const int fd = ::open(dev.device_path->c_str(),
                                  O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) {
                if (require_capture_capability) {
                    return fail(
                        std::error_code(errno, std::generic_category()));
                }
            } else {
                const linux_platform::file_descriptor owned_fd(fd);
                static_cast<void>(owned_fd);
                struct v4l2_capability cap;
                std::memset(&cap, 0, sizeof(cap));
                int ioctl_result = -1;
                do {
                    ioctl_result = ::ioctl(fd, VIDIOC_QUERYCAP, &cap);
                } while (ioctl_result != 0 && errno == EINTR);
                const int ioctl_error = ioctl_result == 0 ? 0 : errno;
                if (ioctl_result == 0) {
                    const std::uint32_t device_caps =
                        (cap.capabilities & V4L2_CAP_DEVICE_CAPS)
                            ? cap.device_caps
                            : cap.capabilities;

                    ::syscape::camera::camera_capabilities caps;
                    caps.has_video_capture =
                        (device_caps & (V4L2_CAP_VIDEO_CAPTURE |
                                        V4L2_CAP_VIDEO_CAPTURE_MPLANE)) != 0;
                    caps.has_video_output =
                        (device_caps & (V4L2_CAP_VIDEO_OUTPUT |
                                        V4L2_CAP_VIDEO_OUTPUT_MPLANE)) != 0;
#ifdef V4L2_CAP_META_CAPTURE
                    caps.has_metadata_capture =
                        (device_caps & V4L2_CAP_META_CAPTURE) != 0;
#endif
#ifdef V4L2_CAP_STREAMING
                    caps.has_streaming =
                        (device_caps & V4L2_CAP_STREAMING) != 0;
#endif
#ifdef V4L2_CAP_TOUCH
                    caps.has_touch_device = (device_caps & V4L2_CAP_TOUCH) != 0;
#endif
                    dev.capabilities = caps;

                    const auto driver_str =
                        v4l2_text(cap.driver, sizeof(cap.driver));
                    const auto card_str = v4l2_text(cap.card, sizeof(cap.card));
                    const auto bus_str =
                        v4l2_text(cap.bus_info, sizeof(cap.bus_info));
                    if (!driver_str || !card_str || !bus_str) {
                        return fail(!driver_str ? driver_str.error()
                                    : !card_str ? card_str.error()
                                                : bus_str.error());
                    }
                    if (driver_str->has_value()) {
                        dev.driver = **driver_str;
                    }
                    if (card_str->has_value()) {
                        dev.card = **card_str;
                    }
                    if (bus_str->has_value()) {
                        dev.bus_info = **bus_str;
                    }
                } else if (require_capture_capability) {
                    return fail(
                        std::error_code(ioctl_error, std::generic_category()));
                }
            }
        }

        // Check parent USB device attributes if present
        const std::string parent_usb = sysfs_entry + "/device/..";
        const auto hw_id = parse_usb_ids(parent_usb);
        if (!hw_id) {
            return fail(hw_id.error());
        }
        if (hw_id->has_value()) {
            dev.hardware_id = **hw_id;
            dev.connection = ::syscape::camera::camera_connection::usb;
        }

        const auto removable_str = read_sysfs_string(parent_usb + "/removable");
        if (!removable_str) {
            return fail(removable_str.error());
        }
        if (removable_str->has_value()) {
            if (**removable_str == "fixed") {
                dev.is_integrated = true;
            } else if (**removable_str == "removable") {
                dev.is_integrated = false;
            }
        }

        // Check physical location panel orientation if exposed in sysfs
        const auto panel_str =
            read_sysfs_string(parent_usb + "/physical_location/panel");
        if (!panel_str) {
            return fail(panel_str.error());
        }
        if (panel_str->has_value()) {
            dev.facing = detect_facing_from_panel(**panel_str);
        } else {
            const auto panel_direct = read_sysfs_string(
                sysfs_entry + "/device/physical_location/panel");
            if (!panel_direct) {
                return fail(panel_direct.error());
            }
            if (panel_direct->has_value()) {
                dev.facing = detect_facing_from_panel(**panel_direct);
            }
        }

        device_list.push_back(std::move(dev));
    }

    // Keep enumeration stable for a fixed set of sysfs entries.
    std::sort(device_list.begin(), device_list.end(),
              [](const ::syscape::camera::camera_device& a,
                 const ::syscape::camera::camera_device& b) {
                  return camera_common::natural_less(a.id, b.id);
              });

    return device_list;
}

inline result<std::vector<::syscape::camera::camera_device>> devices() {
    return enumerate_devices(false);
}

inline result<std::size_t> device_count() {
    const auto res = devices();
    if (!res) {
        return fail(res.error());
    }
    return res->size();
}

inline result<std::vector<::syscape::camera::camera_device>> capture_devices() {
    const auto res = enumerate_devices(true);
    if (!res) {
        return fail(res.error());
    }
    return camera_common::filter_capture_devices(*res);
}

inline result<::syscape::camera::camera_device> default_device() {
    return fail(errc::not_supported);
}

} // namespace camera_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_CAMERA_LINUX_HPP
