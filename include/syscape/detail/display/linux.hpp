#ifndef SYSCAPE_DETAIL_DISPLAY_LINUX_HPP
#define SYSCAPE_DETAIL_DISPLAY_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <syscape/detail/display/common.hpp>
#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/display.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace display_backend {

/// Root path to DRM class directory in sysfs.
constexpr const char* drm_class_root = "/sys/class/drm/";

/// Checks if an entry name matches a DRM connector directory format (e.g. "card0-DP-1", "card1-eDP-1").
inline bool is_connector_name(std::string_view name) noexcept {
    if (name.size() <= 6U || name.substr(0, 4) != "card") {
        return false;
    }
    // Must contain a hyphen separating card number from connector type (e.g. card0-DP-1)
    const std::size_t dash = name.find('-');
    if (dash == std::string_view::npos || dash <= 4U) {
        return false;
    }
    // Characters between "card" and first '-' must be digits
    for (std::size_t i = 4U; i < dash; ++i) {
        if (name[i] < '0' || name[i] > '9') {
            return false;
        }
    }
    return true;
}

/// Inspects and parses a single DRM connector directory.
inline result<::syscape::display::display_info> inspect_connector(
    const std::string& conn_id, const std::string& conn_path) {
    ::syscape::display::display_info info;
    info.id = conn_id;
    if (!is_valid_utf8(info.id)) {
        return fail(errc::invalid_encoding);
    }

    // Classify connector type and internal flag from connector ID
    display_common::classify_connector_name(
        info.id, info.connector_type, info.is_internal);

    // Read connection status (/sys/class/drm/<conn>/status)
    const auto status_text = linux_platform::read_text_file(
        (conn_path + "/status").c_str(), 64U);
    if (status_text) {
        const std::string_view trimmed = display_common::trim_whitespace(*status_text);
        if (trimmed == "connected") {
            info.state = ::syscape::display::connection_state::connected;
        } else if (trimmed == "disconnected") {
            info.state = ::syscape::display::connection_state::disconnected;
        } else {
            info.state = ::syscape::display::connection_state::unknown;
        }
    } else if (status_text.error() != std::errc::no_such_file_or_directory) {
        return fail(status_text.error());
    }

    // Read supported modes (/sys/class/drm/<conn>/modes)
    const auto modes_text = linux_platform::read_text_file(
        (conn_path + "/modes").c_str(), 4096U);
    if (modes_text) {
        std::string_view text(*modes_text);
        while (!text.empty()) {
            const std::size_t newline = text.find('\n');
            std::string_view line = (newline != std::string_view::npos)
                                        ? text.substr(0, newline)
                                        : text;
            if (newline != std::string_view::npos) {
                text.remove_prefix(newline + 1U);
            } else {
                text = std::string_view();
            }
            line = display_common::trim_whitespace(line);
            if (line.empty()) { continue; }

            const auto parsed = display_common::parse_mode_resolution(line);
            if (parsed) {
                const auto duplicate = std::find_if(
                    info.supported_modes.begin(), info.supported_modes.end(),
                    [&parsed](const auto& mode) {
                        return mode.width == parsed->first &&
                               mode.height == parsed->second;
                    });
                if (duplicate == info.supported_modes.end()) {
                    ::syscape::display::display_mode mode;
                    mode.width = parsed->first;
                    mode.height = parsed->second;
                    info.supported_modes.push_back(mode);
                }
            }
        }
    } else if (modes_text.error() != std::errc::no_such_file_or_directory) {
        return fail(modes_text.error());
    }

    // Read binary EDID (/sys/class/drm/<conn>/edid)
    const auto edid_data = linux_platform::read_text_file(
        (conn_path + "/edid").c_str(), display_common::maximum_edid_size);
    if (edid_data && !edid_data->empty()) {
        const auto* raw_bytes = reinterpret_cast<const std::uint8_t*>(edid_data->data());
        const auto edid_res = display_common::parse_edid_block(raw_bytes, edid_data->size());
        if (!edid_res) {
            return fail(edid_res.error());
        }
        if (edid_res->monitor_name.has_value()) {
            info.name = edid_res->monitor_name;
        }
        if (edid_res->manufacturer.has_value()) {
            info.manufacturer = edid_res->manufacturer;
        }
        if (edid_res->physical_width_mm.has_value()) {
            info.physical_width_mm = edid_res->physical_width_mm;
        }
        if (edid_res->physical_height_mm.has_value()) {
            info.physical_height_mm = edid_res->physical_height_mm;
        }
        if (edid_res->preferred_refresh_rate_hz.has_value() &&
            edid_res->preferred_width.has_value() &&
            edid_res->preferred_height.has_value()) {
            for (auto& mode : info.supported_modes) {
                if (mode.width == *edid_res->preferred_width &&
                    mode.height == *edid_res->preferred_height) {
                    mode.refresh_rate_hz = edid_res->preferred_refresh_rate_hz;
                    break;
                }
            }
        }
    } else if (!edid_data && edid_data.error() != std::errc::no_such_file_or_directory) {
        return fail(edid_data.error());
    }

    // Connector sysfs does not expose the compositor's current mode, desktop
    // coordinates, scale, orientation, work area, or primary-display choice.
    // Keep those optional facts absent rather than deriving plausible values.
    return info;
}

/// Collects all displays from sysfs DRM connectors.
inline result<std::vector<::syscape::display::display_info>> collect_displays() {
    std::vector<::syscape::display::display_info> list;

    linux_platform::directory_handle drm_dir(drm_class_root);
    if (!drm_dir.valid()) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

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

        if (!is_connector_name(entry_name)) {
            continue;
        }

        const std::string conn_path = std::string(drm_class_root) + std::string(entry_name);
        const auto inspected = inspect_connector(std::string(entry_name), conn_path);
        if (inspected) {
            list.push_back(*inspected);
        } else if (inspected.error() != std::errc::no_such_file_or_directory) {
            return fail(inspected.error());
        }
    }

    // Stably sort displays by connector ID
    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        return a.id < b.id;
    });

    return list;
}

inline result<std::vector<::syscape::display::display_info>> displays() {
    return collect_displays();
}

inline result<std::size_t> display_count() {
    const auto res = collect_displays();
    if (!res) { return fail(res.error()); }
    return res->size();
}

inline result<::syscape::display::display_info> primary_display() {
    const auto res = collect_displays();
    if (!res) { return fail(res.error()); }
    if (res->empty()) { return fail(errc::not_found); }

    for (const auto& d : *res) {
        if (d.is_primary) {
            return d;
        }
    }
    return fail(errc::not_found);
}

} // namespace display_backend
} // namespace detail
} // namespace syscape

#endif
