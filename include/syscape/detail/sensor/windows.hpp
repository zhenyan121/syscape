#ifndef SYSCAPE_DETAIL_SENSOR_WINDOWS_HPP
#define SYSCAPE_DETAIL_SENSOR_WINDOWS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <syscape/detail/sensor/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace sensor_backend {

namespace scomm = ::syscape::detail::sensor_common;

/// Converts a WMI / ACPI temperature in tenths of Kelvin to degrees Celsius.
inline double acpi_tenths_kelvin_to_celsius(std::uint32_t raw_kelvin) noexcept {
    return scomm::tenths_kelvin_to_celsius(static_cast<std::int64_t>(raw_kelvin));
}

namespace windows_impl {

/// Represents a raw ACPI thermal zone record queried from WMI (MSAcpi_ThermalZoneTemperature).
struct msacpi_thermal_zone_record {
    std::string instance_name;
    std::uint32_t current_temperature_tenths_k = 0U;
    std::uint32_t critical_trip_point_tenths_k = 0U;
    std::uint32_t passive_trip_point_tenths_k = 0U;
    bool active = true;
};

inline result<::syscape::sensor::thermal_zone> parse_msacpi_thermal_zone(
    const msacpi_thermal_zone_record& rec) {
    // 0 Kelvin or unphysically high temperature (> 1000 K / ~726 °C) indicates uninitialized or malformed sensor data
    if (rec.current_temperature_tenths_k == 0U ||
        rec.current_temperature_tenths_k > 10000U) {
        return fail(errc::malformed_data);
    }

    ::syscape::sensor::thermal_zone tz;
    tz.type_name = rec.instance_name.empty() ? "ACPI Thermal Zone" : rec.instance_name;
    tz.type = scomm::classify_thermal_zone(tz.type_name);
    tz.current_celsius = acpi_tenths_kelvin_to_celsius(rec.current_temperature_tenths_k);
    if (rec.critical_trip_point_tenths_k > 0U && rec.critical_trip_point_tenths_k <= 10000U) {
        tz.critical_celsius = acpi_tenths_kelvin_to_celsius(rec.critical_trip_point_tenths_k);
    }
    if (rec.passive_trip_point_tenths_k > 0U && rec.passive_trip_point_tenths_k <= 10000U) {
        tz.passive_celsius = acpi_tenths_kelvin_to_celsius(rec.passive_trip_point_tenths_k);
    }
    if (!rec.instance_name.empty()) {
        tz.zone_id = rec.instance_name;
    }
    tz.enabled = rec.active;
    return tz;
}

inline result<::syscape::sensor::temperature_sensor> parse_msacpi_temperature_sensor(
    const msacpi_thermal_zone_record& rec) {
    if (rec.current_temperature_tenths_k == 0U ||
        rec.current_temperature_tenths_k > 10000U) {
        return fail(errc::malformed_data);
    }

    ::syscape::sensor::temperature_sensor s;
    s.label = rec.instance_name.empty() ? "ACPI Thermal Zone" : rec.instance_name;
    s.type = scomm::classify_temperature_sensor("acpi", s.label);
    s.current_celsius = acpi_tenths_kelvin_to_celsius(rec.current_temperature_tenths_k);
    if (rec.critical_trip_point_tenths_k > 0U && rec.critical_trip_point_tenths_k <= 10000U) {
        s.critical_celsius = acpi_tenths_kelvin_to_celsius(rec.critical_trip_point_tenths_k);
    }
    if (rec.passive_trip_point_tenths_k > 0U && rec.passive_trip_point_tenths_k <= 10000U) {
        s.max_celsius = acpi_tenths_kelvin_to_celsius(rec.passive_trip_point_tenths_k);
    }
    s.chip_name = "ACPI";
    if (!rec.instance_name.empty()) {
        s.device_id = rec.instance_name;
    }
    return s;
}

} // namespace windows_impl

inline result<std::vector<::syscape::sensor::temperature_sensor>> temperatures() {
    // Windows requires administrative privileges and WMI ACPI queries
    // which are not available without elevated system access.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::sensor::fan_sensor>> fans() {
    // Windows standard Win32 APIs do not expose hardware fan speed probes.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::sensor::thermal_zone>> thermal_zones() {
    // Windows standard Win32 APIs do not expose active thermal zone list without WMI ACPI provider.
    return fail(errc::not_supported);
}

} // namespace sensor_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_SENSOR_WINDOWS_HPP
