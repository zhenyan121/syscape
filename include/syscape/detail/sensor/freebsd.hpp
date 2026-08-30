#ifndef SYSCAPE_DETAIL_SENSOR_FREEBSD_HPP
#define SYSCAPE_DETAIL_SENSOR_FREEBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/detail/sensor/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace sensor_backend {

inline result<int> read_sysctl_int(const char* name) {
    int value = 0;
    std::size_t size = sizeof(value);
    if (::sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    return value;
}

inline result<std::vector<::syscape::sensor::thermal_zone>> thermal_zones() {
    std::vector<::syscape::sensor::thermal_zone> zones;
    for (int i = 0; i < 32; ++i) {
        const std::string name =
            "hw.acpi.thermal.tz" + std::to_string(i) + ".temperature";
        const result<int> temp_dk = read_sysctl_int(name.c_str());
        if (!temp_dk) {
            if (i == 0 && temp_dk.error() == errc::not_found) {
                break;
            }
            continue;
        }

        ::syscape::sensor::thermal_zone zone;
        zone.type_name = "acpitz";
        zone.type = sensor_common::classify_thermal_zone("acpitz");
        zone.current_celsius =
            sensor_common::tenths_kelvin_to_celsius(*temp_dk);
        zone.zone_id = "tz" + std::to_string(i);
        zone.enabled = true;

        const std::string crt_name =
            "hw.acpi.thermal.tz" + std::to_string(i) + "._CRT";
        const result<int> crt_dk = read_sysctl_int(crt_name.c_str());
        if (crt_dk) {
            zone.critical_celsius =
                sensor_common::tenths_kelvin_to_celsius(*crt_dk);
        }

        const std::string psv_name =
            "hw.acpi.thermal.tz" + std::to_string(i) + "._PSV";
        const result<int> psv_dk = read_sysctl_int(psv_name.c_str());
        if (psv_dk) {
            zone.passive_celsius =
                sensor_common::tenths_kelvin_to_celsius(*psv_dk);
        }

        zones.push_back(std::move(zone));
    }
    return zones;
}

inline result<std::vector<::syscape::sensor::temperature_sensor>>
temperatures() {
    std::vector<::syscape::sensor::temperature_sensor> sensors;

    // 1. Probe CPU temperature sensors (coretemp / amdtemp)
    for (int i = 0; i < 256; ++i) {
        const std::string name =
            "dev.cpu." + std::to_string(i) + ".temperature";
        const result<int> temp_dk = read_sysctl_int(name.c_str());
        if (!temp_dk) {
            if (i == 0 && temp_dk.error() == errc::not_found) {
                break;
            }
            continue;
        }

        ::syscape::sensor::temperature_sensor sensor;
        sensor.label = "CPU " + std::to_string(i);
        sensor.type = ::syscape::sensor::temperature_sensor_type::cpu;
        sensor.current_celsius =
            sensor_common::tenths_kelvin_to_celsius(*temp_dk);
        sensor.chip_name = "cpu_temp";
        sensor.device_id = "cpu" + std::to_string(i);
        sensors.push_back(std::move(sensor));
    }

    // 2. If no CPU temp, probe ACPI thermal zones
    if (sensors.empty()) {
        const result<std::vector<::syscape::sensor::thermal_zone>> tz_res =
            thermal_zones();
        if (tz_res) {
            for (const auto& tz : *tz_res) {
                ::syscape::sensor::temperature_sensor sensor;
                sensor.label = tz.zone_id.value_or("acpitz");
                sensor.type =
                    ::syscape::sensor::temperature_sensor_type::motherboard;
                sensor.current_celsius = tz.current_celsius;
                sensor.critical_celsius = tz.critical_celsius;
                sensor.chip_name = "acpi";
                sensor.device_id = tz.zone_id;
                sensors.push_back(std::move(sensor));
            }
        }
    }

    return sensors;
}

inline result<std::vector<::syscape::sensor::fan_sensor>> fans() {
    return std::vector<::syscape::sensor::fan_sensor>();
}

} // namespace sensor_backend
} // namespace detail
} // namespace syscape

#endif
