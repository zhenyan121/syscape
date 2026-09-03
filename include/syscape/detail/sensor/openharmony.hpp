#ifndef SYSCAPE_DETAIL_SENSOR_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_SENSOR_OPENHARMONY_HPP

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <dirent.h>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/detail/openharmony/directory.hpp>
#include <syscape/detail/openharmony/file.hpp>
#include <syscape/detail/sensor/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace sensor_backend {

inline result<std::vector<::syscape::sensor::thermal_zone>> thermal_zones() {
    openharmony::directory_handle dir("/sys/class/thermal");
    if (!dir.valid()) {
        if (dir.error() == ENOENT) {
            return fail(errc::not_supported);
        }
        if (dir.error() == EACCES || dir.error() == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    std::vector<::syscape::sensor::thermal_zone> zones;
    for (;;) {
        errno = 0;
        struct dirent* entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        std::string_view name = entry->d_name;
        if (name.rfind("thermal_zone", 0) != 0) {
            continue;
        }

        const std::string dir_path =
            std::string("/sys/class/thermal/") + entry->d_name;
        const std::string type_path = dir_path + "/type";
        const auto type_content =
            openharmony::read_text_file(type_path.c_str());
        if (!type_content) {
            if (type_content.error() == errc::not_found) {
                continue;
            }
            return fail(type_content.error());
        }

        std::string_view type_sv = *type_content;
        openharmony::strip_trailing_newlines(type_sv);
        while (!type_sv.empty() &&
               (type_sv.front() == ' ' || type_sv.front() == '\t')) {
            type_sv.remove_prefix(1U);
        }
        while (!type_sv.empty() &&
               (type_sv.back() == ' ' || type_sv.back() == '\t')) {
            type_sv.remove_suffix(1U);
        }
        if (type_sv.empty()) {
            return fail(errc::malformed_data);
        }

        const std::string temp_path = dir_path + "/temp";
        const auto temp_content =
            openharmony::read_text_file(temp_path.c_str());
        if (!temp_content) {
            if (temp_content.error() == errc::not_found) {
                continue;
            }
            return fail(temp_content.error());
        }

        std::string_view temp_sv = *temp_content;
        openharmony::strip_trailing_newlines(temp_sv);
        while (!temp_sv.empty() &&
               (temp_sv.front() == ' ' || temp_sv.front() == '\t')) {
            temp_sv.remove_prefix(1U);
        }
        while (!temp_sv.empty() &&
               (temp_sv.back() == ' ' || temp_sv.back() == '\t')) {
            temp_sv.remove_suffix(1U);
        }
        if (temp_sv.empty()) {
            return fail(errc::malformed_data);
        }

        std::int64_t millidegrees = 0;
        const auto r = std::from_chars(
            temp_sv.data(), temp_sv.data() + temp_sv.size(), millidegrees);
        if (r.ec == std::errc::result_out_of_range) {
            return fail(errc::value_too_large);
        }
        if (r.ec != std::errc() || r.ptr != temp_sv.data() + temp_sv.size()) {
            return fail(errc::malformed_data);
        }

        ::syscape::sensor::thermal_zone zone {};
        zone.type_name = std::string(type_sv);
        zone.zone_id = entry->d_name;
        zone.type = sensor_common::classify_thermal_zone(zone.type_name);
        zone.current_celsius = static_cast<double>(millidegrees) / 1000.0;

        const auto val_res = sensor_common::validate_thermal_zone(zone);
        if (!val_res) {
            return fail(val_res.error());
        }

        zones.push_back(std::move(zone));
    }

    return zones;
}

inline result<std::vector<::syscape::sensor::temperature_sensor>>
temperatures() {
    const auto zones = thermal_zones();
    if (!zones) {
        return fail(zones.error());
    }

    std::vector<::syscape::sensor::temperature_sensor> sensors;
    sensors.reserve(zones->size());
    for (const auto& zone : *zones) {
        ::syscape::sensor::temperature_sensor rec {};
        rec.device_id = zone.zone_id;
        rec.label = zone.type_name;
        rec.current_celsius = zone.current_celsius;
        rec.type = sensor_common::classify_temperature_sensor(
            std::string_view(), zone.type_name);
        const auto val_res = sensor_common::validate_temperature_sensor(rec);
        if (!val_res) {
            return fail(val_res.error());
        }
        sensors.push_back(std::move(rec));
    }
    return sensors;
}

inline result<std::vector<::syscape::sensor::fan_sensor>> fans() {
    return fail(errc::not_supported);
}

} // namespace sensor_backend
} // namespace detail
} // namespace syscape

#endif
