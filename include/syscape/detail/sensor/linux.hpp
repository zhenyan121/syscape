#ifndef SYSCAPE_DETAIL_SENSOR_LINUX_HPP
#define SYSCAPE_DETAIL_SENSOR_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <dirent.h>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/stat.h>
#include <vector>

#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/sensor/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace sensor_backend {

namespace lplat = ::syscape::detail::linux_platform;
namespace scomm = ::syscape::detail::sensor_common;

constexpr const char* default_hwmon_root = "/sys/class/hwmon";
constexpr const char* default_thermal_root = "/sys/class/thermal";

inline std::string_view trim_attribute(std::string_view value) noexcept {
    const auto blank = [](char letter) noexcept {
        return letter == ' ' || letter == '\t' || letter == '\r' ||
               letter == '\n';
    };
    while (!value.empty() && blank(value.front())) { value.remove_prefix(1U); }
    while (!value.empty() && blank(value.back())) { value.remove_suffix(1U); }
    return value;
}

inline result<std::int64_t> parse_millicelsius(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value.empty()) {
        return fail(errc::malformed_data);
    }
    std::int64_t parsed_val = 0;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result res = std::from_chars(first, last, parsed_val);
    if (res.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (res.ec != std::errc() || res.ptr != last) {
        return fail(errc::malformed_data);
    }
    return parsed_val;
}

inline result<std::uint32_t> parse_rpm(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value.empty()) {
        return fail(errc::malformed_data);
    }
    std::uint32_t parsed_val = 0U;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result res = std::from_chars(first, last, parsed_val);
    if (res.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (res.ec != std::errc() || res.ptr != last) {
        return fail(errc::malformed_data);
    }
    return parsed_val;
}

inline bool is_missing_attribute(const std::error_code& error) noexcept {
    return error == std::error_code(ENOENT, std::generic_category());
}

/// Reads an optional sysfs attribute while preserving failures other than absence.
inline result<std::optional<std::string>> read_optional_attribute_string(
    const std::string& path, std::size_t maximum_size = 1024U) {
    const auto content = lplat::read_text_file(path.c_str(), maximum_size);
    if (!content) {
        if (is_missing_attribute(content.error())) {
            return std::optional<std::string>();
        }
        return fail(content.error());
    }
    return std::optional<std::string>(
        std::string(trim_attribute(*content)));
}

/// Helper to read a chip name from hwmon directory (hwmonX/name or hwmonX/device/name).
inline result<std::string> read_chip_name(const std::string& hwmon_dir) {
    const auto name = read_optional_attribute_string(hwmon_dir + "/name");
    if (!name) {
        return fail(name.error());
    }
    if (*name && !(**name).empty()) {
        return **name;
    }
    const auto dev_name =
        read_optional_attribute_string(hwmon_dir + "/device/name");
    if (!dev_name) {
        return fail(dev_name.error());
    }
    if (*dev_name && !(**dev_name).empty()) {
        return **dev_name;
    }
    return std::string();
}

/// Enumerates temperature sensors from a hwmon directory.
inline result<void> enumerate_hwmon_temperatures_in_dir(
    const std::string& dir_path,
    const std::string& entry_name,
    const std::string& chip_name,
    std::vector<std::string>& seen_attributes,
    std::vector<::syscape::sensor::temperature_sensor>& results) {

    const lplat::directory_handle handle(dir_path.c_str());
    if (!handle.valid()) {
        const std::error_code error(errno, std::generic_category());
        if (is_missing_attribute(error) ||
            error == std::error_code(ENOTDIR, std::generic_category())) {
            return {};
        }
        return fail(error);
    }

    for (;;) {
        errno = 0;
        struct ::dirent* entry = ::readdir(handle.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        std::string_view name(entry->d_name);
        // Look for temp[0-9]+_input
        if (!name.rfind("temp", 0) && name.length() > 10 &&
            name.substr(name.length() - 6) == "_input") {
            const std::string_view num_part = name.substr(4, name.length() - 10);
            if (num_part.empty()) {
                continue;
            }
            bool all_digits = true;
            for (char c : num_part) {
                if (c < '0' || c > '9') {
                    all_digits = false;
                    break;
                }
            }
            if (!all_digits) {
                continue;
            }
            const std::string attribute_id(num_part);
            if (std::find(seen_attributes.begin(), seen_attributes.end(),
                          attribute_id) != seen_attributes.end()) {
                continue;
            }

            const std::string input_path = dir_path + "/" + std::string(name);
            const auto temp_raw = lplat::read_text_file(input_path.c_str(), 128U);
            if (!temp_raw) {
                if (is_missing_attribute(temp_raw.error())) {
                    continue;
                }
                return fail(temp_raw.error());
            }
            const auto parsed_milli = parse_millicelsius(*temp_raw);
            if (!parsed_milli) {
                return fail(parsed_milli.error());
            }

            ::syscape::sensor::temperature_sensor sensor;
            sensor.current_celsius = scomm::millicelsius_to_celsius(*parsed_milli);
            sensor.device_id = entry_name;
            if (!chip_name.empty()) {
                sensor.chip_name = chip_name;
            }

            // Check for tempX_label
            const std::string label_path = dir_path + "/temp" + std::string(num_part) + "_label";
            const auto label_read = read_optional_attribute_string(label_path);
            if (!label_read) {
                return fail(label_read.error());
            }
            if (*label_read && !(**label_read).empty()) {
                sensor.label = **label_read;
            } else if (!chip_name.empty()) {
                sensor.label = chip_name + " temp" + std::string(num_part);
            } else {
                sensor.label = "temp" + std::string(num_part);
            }

            // Check for tempX_max
            const std::string max_path = dir_path + "/temp" + std::string(num_part) + "_max";
            const auto max_read = read_optional_attribute_string(max_path, 128U);
            if (!max_read) {
                return fail(max_read.error());
            }
            if (*max_read) {
                const auto parsed_max = parse_millicelsius(**max_read);
                if (!parsed_max) {
                    return fail(parsed_max.error());
                }
                sensor.max_celsius = scomm::millicelsius_to_celsius(*parsed_max);
            }

            // Check for tempX_crit
            const std::string crit_path = dir_path + "/temp" + std::string(num_part) + "_crit";
            const auto crit_read = read_optional_attribute_string(crit_path, 128U);
            if (!crit_read) {
                return fail(crit_read.error());
            }
            if (*crit_read) {
                const auto parsed_crit = parse_millicelsius(**crit_read);
                if (!parsed_crit) {
                    return fail(parsed_crit.error());
                }
                sensor.critical_celsius = scomm::millicelsius_to_celsius(*parsed_crit);
            }

            sensor.type = scomm::classify_temperature_sensor(
                sensor.chip_name ? *sensor.chip_name : "",
                sensor.label);

            const auto val_res = scomm::validate_temperature_sensor(sensor);
            if (!val_res) {
                return fail(val_res.error());
            }

            results.push_back(std::move(sensor));
            seen_attributes.push_back(attribute_id);
        }
    }
    return {};
}

/// Enumerates fan sensors from a hwmon directory.
inline result<void> enumerate_hwmon_fans_in_dir(
    const std::string& dir_path,
    const std::string& entry_name,
    const std::string& chip_name,
    std::vector<std::string>& seen_attributes,
    std::vector<::syscape::sensor::fan_sensor>& results) {

    const lplat::directory_handle handle(dir_path.c_str());
    if (!handle.valid()) {
        const std::error_code error(errno, std::generic_category());
        if (is_missing_attribute(error) ||
            error == std::error_code(ENOTDIR, std::generic_category())) {
            return {};
        }
        return fail(error);
    }

    for (;;) {
        errno = 0;
        struct ::dirent* entry = ::readdir(handle.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        std::string_view name(entry->d_name);
        // Look for fan[0-9]+_input
        if (!name.rfind("fan", 0) && name.length() > 9 &&
            name.substr(name.length() - 6) == "_input") {
            const std::string_view num_part = name.substr(3, name.length() - 9);
            if (num_part.empty()) {
                continue;
            }
            bool all_digits = true;
            for (char c : num_part) {
                if (c < '0' || c > '9') {
                    all_digits = false;
                    break;
                }
            }
            if (!all_digits) {
                continue;
            }
            const std::string attribute_id(num_part);
            if (std::find(seen_attributes.begin(), seen_attributes.end(),
                          attribute_id) != seen_attributes.end()) {
                continue;
            }

            const std::string input_path = dir_path + "/" + std::string(name);
            const auto fan_raw = lplat::read_text_file(input_path.c_str(), 128U);
            if (!fan_raw) {
                if (is_missing_attribute(fan_raw.error())) {
                    continue;
                }
                return fail(fan_raw.error());
            }
            const auto parsed_rpm = parse_rpm(*fan_raw);
            if (!parsed_rpm) {
                return fail(parsed_rpm.error());
            }

            ::syscape::sensor::fan_sensor fan;
            fan.current_rpm = *parsed_rpm;
            fan.device_id = entry_name;
            if (!chip_name.empty()) {
                fan.chip_name = chip_name;
            }

            // Check for fanX_label
            const std::string label_path = dir_path + "/fan" + std::string(num_part) + "_label";
            const auto label_read = read_optional_attribute_string(label_path);
            if (!label_read) {
                return fail(label_read.error());
            }
            if (*label_read && !(**label_read).empty()) {
                fan.label = **label_read;
            } else if (!chip_name.empty()) {
                fan.label = chip_name + " fan" + std::string(num_part);
            } else {
                fan.label = "fan" + std::string(num_part);
            }

            // Check for fanX_min
            const std::string min_path = dir_path + "/fan" + std::string(num_part) + "_min";
            const auto min_read = read_optional_attribute_string(min_path, 128U);
            if (!min_read) {
                return fail(min_read.error());
            }
            if (*min_read) {
                const auto parsed_min = parse_rpm(**min_read);
                if (!parsed_min) {
                    return fail(parsed_min.error());
                }
                fan.min_rpm = *parsed_min;
            }

            // Check for fanX_max
            const std::string max_path = dir_path + "/fan" + std::string(num_part) + "_max";
            const auto max_read = read_optional_attribute_string(max_path, 128U);
            if (!max_read) {
                return fail(max_read.error());
            }
            if (*max_read) {
                const auto parsed_max = parse_rpm(**max_read);
                if (!parsed_max) {
                    return fail(parsed_max.error());
                }
                fan.max_rpm = *parsed_max;
            }

            // Check for fanX_target
            const std::string target_path = dir_path + "/fan" + std::string(num_part) + "_target";
            const auto target_read = read_optional_attribute_string(target_path, 128U);
            if (!target_read) {
                return fail(target_read.error());
            }
            if (*target_read) {
                const auto parsed_target = parse_rpm(**target_read);
                if (!parsed_target) {
                    return fail(parsed_target.error());
                }
                fan.target_rpm = *parsed_target;
            }

            const auto val_res = scomm::validate_fan_sensor(fan);
            if (!val_res) {
                return fail(val_res.error());
            }

            results.push_back(std::move(fan));
            seen_attributes.push_back(attribute_id);
        }
    }
    return {};
}

/// Enumerates temperature sensors from custom hwmon and thermal roots.
inline result<std::vector<::syscape::sensor::temperature_sensor>> temperatures_at(
    const std::string& hwmon_root) {
    struct ::stat st {};
    if (::stat(hwmon_root.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    const lplat::directory_handle handle(hwmon_root.c_str());
    if (!handle.valid()) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    std::vector<::syscape::sensor::temperature_sensor> results;
    for (;;) {
        errno = 0;
        struct ::dirent* entry = ::readdir(handle.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }
        const std::string entry_name = entry->d_name;
        const std::string hwmon_dir = hwmon_root + "/" + entry_name;
        const auto chip_name = read_chip_name(hwmon_dir);
        if (!chip_name) {
            if (is_missing_attribute(chip_name.error())) {
                continue;
            }
            return fail(chip_name.error());
        }

        std::vector<std::string> seen_attributes;

        // Prefer attributes in the hwmon class device.
        const auto res_direct = enumerate_hwmon_temperatures_in_dir(
            hwmon_dir, entry_name, *chip_name, seen_attributes, results);
        if (!res_direct) {
            return fail(res_direct.error());
        }

        // Also inspect the legacy physical-device layout, deduplicating by
        // the kernel's per-chip temp index when both locations expose it.
        const std::string device_dir = hwmon_dir + "/device";
        const auto res_device = enumerate_hwmon_temperatures_in_dir(
            device_dir, entry_name, *chip_name, seen_attributes, results);
        if (!res_device) {
            return fail(res_device.error());
        }
    }

    return results;
}

/// Enumerates fan sensors from custom hwmon root.
inline result<std::vector<::syscape::sensor::fan_sensor>> fans_at(
    const std::string& hwmon_root) {
    struct ::stat st {};
    if (::stat(hwmon_root.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    const lplat::directory_handle handle(hwmon_root.c_str());
    if (!handle.valid()) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    std::vector<::syscape::sensor::fan_sensor> results;
    for (;;) {
        errno = 0;
        struct ::dirent* entry = ::readdir(handle.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }
        const std::string entry_name = entry->d_name;
        const std::string hwmon_dir = hwmon_root + "/" + entry_name;
        const auto chip_name = read_chip_name(hwmon_dir);
        if (!chip_name) {
            if (is_missing_attribute(chip_name.error())) {
                continue;
            }
            return fail(chip_name.error());
        }

        std::vector<std::string> seen_attributes;

        // Prefer attributes in the hwmon class device.
        const auto res_direct = enumerate_hwmon_fans_in_dir(
            hwmon_dir, entry_name, *chip_name, seen_attributes, results);
        if (!res_direct) {
            return fail(res_direct.error());
        }

        // Also inspect the legacy physical-device layout, deduplicating by
        // the kernel's per-chip fan index when both locations expose it.
        const std::string device_dir = hwmon_dir + "/device";
        const auto res_device = enumerate_hwmon_fans_in_dir(
            device_dir, entry_name, *chip_name, seen_attributes, results);
        if (!res_device) {
            return fail(res_device.error());
        }
    }

    return results;
}

/// Enumerates thermal zones from custom thermal root.
inline result<std::vector<::syscape::sensor::thermal_zone>> thermal_zones_at(
    const std::string& thermal_root) {
    struct ::stat st {};
    if (::stat(thermal_root.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    const lplat::directory_handle handle(thermal_root.c_str());
    if (!handle.valid()) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    std::vector<::syscape::sensor::thermal_zone> results;
    for (;;) {
        errno = 0;
        struct ::dirent* entry = ::readdir(handle.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }
        std::string_view entry_name(entry->d_name);
        if (entry_name.rfind("thermal_zone", 0) != 0) {
            continue;
        }

        const std::string zone_dir = thermal_root + "/" + std::string(entry_name);
        const std::string temp_path = zone_dir + "/temp";
        const auto temp_raw = lplat::read_text_file(temp_path.c_str(), 128U);
        if (!temp_raw) {
            if (is_missing_attribute(temp_raw.error())) {
                continue;
            }
            return fail(temp_raw.error());
        }
        const auto parsed_temp = parse_millicelsius(*temp_raw);
        if (!parsed_temp) {
            return fail(parsed_temp.error());
        }

        ::syscape::sensor::thermal_zone zone;
        zone.zone_id = std::string(entry_name);
        zone.current_celsius = scomm::millicelsius_to_celsius(*parsed_temp);

        // Read zone type
        const std::string type_path = zone_dir + "/type";
        const auto type_read = read_optional_attribute_string(type_path);
        if (!type_read) {
            return fail(type_read.error());
        }
        if (!*type_read) {
            continue;
        }
        if ((**type_read).empty()) {
            return fail(errc::malformed_data);
        }
        zone.type_name = **type_read;
        zone.type = scomm::classify_thermal_zone(zone.type_name);

        // Read zone mode
        const std::string mode_path = zone_dir + "/mode";
        const auto mode_read = read_optional_attribute_string(mode_path);
        if (!mode_read) {
            return fail(mode_read.error());
        }
        if (*mode_read) {
            if (**mode_read == "enabled") {
                zone.enabled = true;
            } else if (**mode_read == "disabled") {
                zone.enabled = false;
            } else {
                return fail(errc::malformed_data);
            }
        }

        // Read trip points (trip_point_0_type, trip_point_0_temp, ...)
        for (int i = 0; i < 32; ++i) {
            const std::string trip_type_path =
                zone_dir + "/trip_point_" + std::to_string(i) + "_type";
            const auto trip_type =
                read_optional_attribute_string(trip_type_path);
            if (!trip_type) {
                return fail(trip_type.error());
            }
            if (!*trip_type) {
                break;
            }
            if ((**trip_type).empty()) {
                return fail(errc::malformed_data);
            }

            const std::string trip_temp_path =
                zone_dir + "/trip_point_" + std::to_string(i) + "_temp";
            const auto trip_temp_raw =
                read_optional_attribute_string(trip_temp_path, 128U);
            if (!trip_temp_raw) {
                return fail(trip_temp_raw.error());
            }
            if (!*trip_temp_raw) {
                continue;
            }
            const auto parsed_trip = parse_millicelsius(**trip_temp_raw);
            if (!parsed_trip) {
                return fail(parsed_trip.error());
            }

            const double trip_celsius = scomm::millicelsius_to_celsius(*parsed_trip);
            if (**trip_type == "passive" && !zone.passive_celsius) {
                zone.passive_celsius = trip_celsius;
            } else if ((**trip_type == "critical" || **trip_type == "hot") &&
                       !zone.critical_celsius) {
                zone.critical_celsius = trip_celsius;
            }
        }

        const auto val_res = scomm::validate_thermal_zone(zone);
        if (!val_res) {
            return fail(val_res.error());
        }

        results.push_back(std::move(zone));
    }

    return results;
}

inline result<std::vector<::syscape::sensor::temperature_sensor>> temperatures() {
    return temperatures_at(default_hwmon_root);
}

inline result<std::vector<::syscape::sensor::fan_sensor>> fans() {
    return fans_at(default_hwmon_root);
}

inline result<std::vector<::syscape::sensor::thermal_zone>> thermal_zones() {
    return thermal_zones_at(default_thermal_root);
}

} // namespace sensor_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_SENSOR_LINUX_HPP
