#ifndef SYSCAPE_DETAIL_GPU_LINUX_HPP
#define SYSCAPE_DETAIL_GPU_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <charconv>
#include <cstdlib>
#include <dirent.h>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/gpu/common.hpp>
#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/gpu.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace gpu_backend {

/// Path to PCI bus devices directory in sysfs.
constexpr const char* pci_devices_root = "/sys/bus/pci/devices/";

/// Path to DRM class directory in sysfs.
constexpr const char* drm_class_root = "/sys/class/drm/";

/// Extracts the basename of a path (everything after the last '/').
inline std::string_view path_basename(std::string_view path) noexcept {
    const std::size_t slash_pos = path.find_last_of('/');
    if (slash_pos != std::string_view::npos) {
        return path.substr(slash_pos + 1U);
    }
    return path;
}

/// Resolves the canonical physical realpath of a sysfs node for deduplication.
inline result<std::string> canonical_path(const std::string& path) {
    char buffer[PATH_MAX];
    const char* res = ::realpath(path.c_str(), buffer);
    if (res == nullptr) {
        if (errno == ENOENT) {
            return std::string();
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    return std::string(res);
}

/// Parses a decimal unsigned 64-bit integer with overflow checks.
inline result<std::uint64_t> parse_decimal_u64(std::string_view input) noexcept {
    const std::string_view trimmed = gpu_common::trim_whitespace(input);
    if (trimmed.empty()) { return fail(errc::malformed_data); }
    std::uint64_t val = 0U;
    const char* first = trimmed.data();
    const char* last = first + trimmed.size();
    const auto parsed = std::from_chars(first, last, val);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() || parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    return val;
}

/// Reads the symlink target basename for a driver or subsystem with strict error propagation.
inline result<std::optional<std::string>> read_driver_name(const std::string& device_dir) {
    const std::string driver_link = device_dir + "/driver";
    char buffer[1024];
    const ssize_t len = ::readlink(driver_link.c_str(), buffer, sizeof(buffer));
    if (len < 0) {
        if (errno == ENOENT || errno == EINVAL) {
            return std::optional<std::string>(std::nullopt);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (static_cast<std::size_t>(len) >= sizeof(buffer)) {
        return fail(errc::value_too_large);
    }
    std::string name(path_basename(std::string_view(buffer, static_cast<std::size_t>(len))));
    if (name.empty()) {
        return std::optional<std::string>(std::nullopt);
    }
    if (!is_valid_utf8(name)) {
        return fail(errc::invalid_encoding);
    }
    return std::optional<std::string>(std::move(name));
}

/// Attempts to read dedicated VRAM in bytes from driver-specific sysfs nodes.
inline result<std::optional<std::uint64_t>> read_vram_bytes(const std::string& device_dir) {
    const std::string vram_file = device_dir + "/mem_info_vram_total";
    const auto amd_vram = linux_platform::read_text_file(vram_file.c_str(), 64U);
    if (amd_vram) {
        const auto parsed = parse_decimal_u64(*amd_vram);
        if (!parsed) {
            return fail(parsed.error());
        }
        return std::optional<std::uint64_t>(*parsed);
    }
    if (amd_vram.error() != std::errc::no_such_file_or_directory &&
        amd_vram.error() != std::errc::operation_not_supported) {
        return fail(amd_vram.error());
    }
    return std::optional<std::uint64_t>(std::nullopt);
}

/// Classifies a GPU vendor from driver name using exact matching where appropriate.
inline ::syscape::gpu::gpu_vendor classify_driver_or_node(std::string_view name) noexcept {
    if (name == "panfrost" || name == "mali" || name == "lima" || name == "mali_kbase") {
        return ::syscape::gpu::gpu_vendor::arm_mali;
    }
    if (name == "vc4" || name == "v3d" || name == "bcm2835" || name == "bcm2711" || name == "bcm2712") {
        return ::syscape::gpu::gpu_vendor::broadcom_videocore;
    }
    if (name == "adreno" || name == "msm" || name == "kgsl" || name == "kgsl-3d0") {
        return ::syscape::gpu::gpu_vendor::qualcomm_adreno;
    }
    if (name == "powervr" || name == "pvr" || name == "pvrsrvkm") {
        return ::syscape::gpu::gpu_vendor::imagination_powervr;
    }
    if (name == "tegra" || name == "tegra-drm" || name == "tegra_drm" || name == "nouveau" || name == "nvidia") {
        return ::syscape::gpu::gpu_vendor::nvidia;
    }
    if (name == "amdgpu" || name == "radeon") {
        return ::syscape::gpu::gpu_vendor::amd;
    }
    if (name == "i915" || name == "xe") {
        return ::syscape::gpu::gpu_vendor::intel;
    }
    if (name == "virtio-gpu" || name == "virtio_gpu") {
        return ::syscape::gpu::gpu_vendor::virtio;
    }
    if (name == "vboxvideo") {
        return ::syscape::gpu::gpu_vendor::other;
    }
    return ::syscape::gpu::gpu_vendor::unknown;
}

/// Reads device details given a backing sysfs device directory path.
inline result<::syscape::gpu::gpu_device> inspect_device(
    const std::string& dev_id, const std::string& dev_path) {
    ::syscape::gpu::gpu_device dev;
    dev.id = dev_id;
    if (!is_valid_utf8(dev.id)) {
        return fail(errc::invalid_encoding);
    }

    // Read vendor ID
    const auto vendor_text = linux_platform::read_text_file(
        (dev_path + "/vendor").c_str(), 64U);
    if (vendor_text) {
        const auto vid = gpu_common::parse_hex(*vendor_text);
        if (!vid) { return fail(vid.error()); }
        dev.vendor_id = *vid;
    } else if (vendor_text.error() != std::errc::no_such_file_or_directory) {
        return fail(vendor_text.error());
    }

    // Read device ID
    const auto device_text = linux_platform::read_text_file(
        (dev_path + "/device").c_str(), 64U);
    if (device_text) {
        const auto did = gpu_common::parse_hex(*device_text);
        if (!did) { return fail(did.error()); }
        dev.device_id = *did;
    } else if (device_text.error() != std::errc::no_such_file_or_directory) {
        return fail(device_text.error());
    }

    // Read driver
    const auto driver_res = read_driver_name(dev_path);
    if (!driver_res) {
        return fail(driver_res.error());
    }
    dev.driver = *driver_res;

    // Classify vendor
    if (dev.vendor_id.has_value()) {
        dev.vendor = gpu_common::classify_pci_vendor_id(*dev.vendor_id);
    } else if (dev.driver.has_value()) {
        dev.vendor = classify_driver_or_node(*dev.driver);
    } else {
        dev.vendor = ::syscape::gpu::gpu_vendor::unknown;
    }
    dev.vendor_name = gpu_common::vendor_to_string(dev.vendor);

    // Read boot_vga
    const auto boot_vga_text = linux_platform::read_text_file(
        (dev_path + "/boot_vga").c_str(), 16U);
    if (boot_vga_text) {
        const std::string_view trimmed = gpu_common::trim_whitespace(*boot_vga_text);
        if (trimmed == "1") {
            dev.is_primary = true;
        } else if (trimmed == "0") {
            dev.is_primary = false;
        } else {
            return fail(errc::malformed_data);
        }
    } else if (boot_vga_text.error() != std::errc::no_such_file_or_directory) {
        return fail(boot_vga_text.error());
    }

    // Read VRAM
    const auto vram_res = read_vram_bytes(dev_path);
    if (!vram_res) {
        return fail(vram_res.error());
    }
    dev.vram_bytes = *vram_res;

    // Read hardware label / model if exposed
    const auto label_text = linux_platform::read_text_file(
        (dev_path + "/label").c_str(), 256U);
    if (label_text) {
        std::string label(gpu_common::trim_whitespace(*label_text));
        if (!label.empty()) {
            if (!is_valid_utf8(label)) {
                return fail(errc::invalid_encoding);
            }
            dev.name = std::move(label);
        }
    } else if (label_text.error() != std::errc::no_such_file_or_directory) {
        return fail(label_text.error());
    }

    if (!dev.name.has_value()) {
        const auto of_node = linux_platform::read_text_file(
            (dev_path + "/of_node/name").c_str(), 256U);
        if (of_node) {
            std::string node_name(gpu_common::trim_whitespace(*of_node));
            if (!node_name.empty()) {
                if (!is_valid_utf8(node_name)) {
                    return fail(errc::invalid_encoding);
                }
                dev.name = std::move(node_name);
            }
        } else if (of_node.error() != std::errc::no_such_file_or_directory) {
            return fail(of_node.error());
        }
    }

    return dev;
}

/// Collects GPU devices from sysfs (both DRM class and PCI bus) with canonical path deduplication.
inline result<std::vector<::syscape::gpu::gpu_device>> collect_devices() {
    std::vector<::syscape::gpu::gpu_device> list;
    std::vector<std::string> seen_canonical_paths;

    bool pci_available = false;
    bool drm_available = false;

    // 1. Enumerate /sys/class/drm/
    linux_platform::directory_handle drm_dir(drm_class_root);
    if (drm_dir.valid()) {
        drm_available = true;
        for (;;) {
            errno = 0;
            const struct ::dirent* entry = ::readdir(drm_dir.get());
            if (entry == nullptr) {
                if (errno != 0) {
                    return fail(std::error_code(errno, std::generic_category()));
                }
                break;
            }
            const std::string_view entry_name(entry->d_name);
            if (entry_name == "." || entry_name == "..") { continue; }

            // Look for card[0-9]+ (exclude renderD*, controlD*, card*-DP-*, etc.)
            if (entry_name.size() <= 4U || entry_name.substr(0, 4) != "card") {
                continue;
            }
            bool is_primary_card_node = true;
            for (std::size_t i = 4U; i < entry_name.size(); ++i) {
                if (entry_name[i] < '0' || entry_name[i] > '9') {
                    is_primary_card_node = false;
                    break;
                }
            }
            if (!is_primary_card_node) { continue; }

            const std::string card_path = std::string(drm_class_root) + std::string(entry_name);
            const std::string dev_link = card_path + "/device";

            const auto canon = canonical_path(dev_link);
            if (!canon) {
                return fail(canon.error());
            }

            const std::string target_dev_path = canon->empty() ? card_path : *canon;
            if (!target_dev_path.empty()) {
                seen_canonical_paths.push_back(target_dev_path);
            }

            const auto inspected = inspect_device(std::string(entry_name), target_dev_path);
            if (inspected) {
                list.push_back(*inspected);
            } else if (inspected.error() != std::errc::no_such_file_or_directory) {
                return fail(inspected.error());
            }
        }
    } else if (errno != ENOENT) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    // 2. Enumerate /sys/bus/pci/devices/
    linux_platform::directory_handle pci_dir(pci_devices_root);
    if (pci_dir.valid()) {
        pci_available = true;
        for (;;) {
            errno = 0;
            const struct ::dirent* entry = ::readdir(pci_dir.get());
            if (entry == nullptr) {
                if (errno != 0) {
                    return fail(std::error_code(errno, std::generic_category()));
                }
                break;
            }
            const std::string_view entry_name(entry->d_name);
            if (entry_name == "." || entry_name == "..") { continue; }

            const std::string dev_path = std::string(pci_devices_root) + std::string(entry_name);

            // Read PCI class
            const auto class_text = linux_platform::read_text_file(
                (dev_path + "/class").c_str(), 64U);
            if (!class_text) {
                if (class_text.error() == std::errc::no_such_file_or_directory) {
                    continue;
                }
                return fail(class_text.error());
            }

            const auto pci_class = gpu_common::parse_hex(*class_text);
            if (!pci_class) {
                return fail(pci_class.error());
            }

            // PCI Base Class 0x03 is Display Controller
            if ((*pci_class >> 16U) != 0x03U) {
                continue;
            }

            // Check if already enumerated via DRM using canonical realpath
            const auto canon = canonical_path(dev_path);
            if (!canon) {
                return fail(canon.error());
            }

            bool already_seen = false;
            for (const auto& seen : seen_canonical_paths) {
                if (seen == *canon) {
                    already_seen = true;
                    break;
                }
            }
            if (already_seen) { continue; }

            const auto inspected = inspect_device(std::string(entry_name), dev_path);
            if (inspected) {
                list.push_back(*inspected);
            } else if (inspected.error() != std::errc::no_such_file_or_directory) {
                return fail(inspected.error());
            }
        }
    } else if (errno != ENOENT) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    if (!pci_available && !drm_available) {
        return fail(errc::not_supported);
    }

    // Sort devices stably by id
    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        return a.id < b.id;
    });

    return list;
}

inline result<std::vector<::syscape::gpu::gpu_device>> devices() {
    return collect_devices();
}

inline result<std::size_t> device_count() {
    const auto res = collect_devices();
    if (!res) { return fail(res.error()); }
    return res->size();
}

inline result<::syscape::gpu::gpu_device> primary_device() {
    const auto res = collect_devices();
    if (!res) { return fail(res.error()); }
    if (res->empty()) { return fail(errc::not_found); }

    for (const auto& dev : *res) {
        if (dev.is_primary.has_value() && *dev.is_primary) {
            return dev;
        }
    }
    return fail(errc::not_found);
}

} // namespace gpu_backend
} // namespace detail
} // namespace syscape

#endif
