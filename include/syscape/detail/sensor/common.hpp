#ifndef SYSCAPE_DETAIL_SENSOR_COMMON_HPP
#define SYSCAPE_DETAIL_SENSOR_COMMON_HPP

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace sensor_common {

/// Converts a temperature measurement in millidegrees Celsius to degrees Celsius.
inline double millicelsius_to_celsius(std::int64_t millicelsius) noexcept {
    return static_cast<double>(millicelsius) / 1000.0;
}

/// Converts a temperature measurement in tenths of Kelvin (decikelvin) to degrees Celsius.
inline double tenths_kelvin_to_celsius(std::int64_t tenths_kelvin) noexcept {
    return static_cast<double>(tenths_kelvin) / 10.0 - 273.15;
}

/// Case-insensitive substring search.
inline bool contains_ignore_case(
    std::string_view text, std::string_view needle) noexcept {
    if (needle.empty()) {
        return true;
    }
    if (text.size() < needle.size()) {
        return false;
    }
    for (std::size_t i = 0; i <= text.size() - needle.size(); ++i) {
        bool matches = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const auto a = static_cast<unsigned char>(text[i + j]);
            const auto b = static_cast<unsigned char>(needle[j]);
            if (std::tolower(a) != std::tolower(b)) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }
    return false;
}

/// Heuristically classifies a temperature sensor based on chip and label strings.
inline ::syscape::sensor::temperature_sensor_type classify_temperature_sensor(
    std::string_view chip, std::string_view label) noexcept {
    using sensor_type = ::syscape::sensor::temperature_sensor_type;

    // CPU hints
    if (contains_ignore_case(chip, "coretemp") ||
        contains_ignore_case(chip, "k10temp") ||
        contains_ignore_case(chip, "zenpower") ||
        contains_ignore_case(chip, "via_cputemp") ||
        contains_ignore_case(label, "cpu") ||
        contains_ignore_case(label, "core") ||
        contains_ignore_case(label, "tctl") ||
        contains_ignore_case(label, "tdie") ||
        contains_ignore_case(label, "pkg_temp") ||
        contains_ignore_case(label, "package id")) {
        return sensor_type::cpu;
    }

    // GPU hints
    if (contains_ignore_case(chip, "amdgpu") ||
        contains_ignore_case(chip, "nouveau") ||
        contains_ignore_case(chip, "nvidia") ||
        contains_ignore_case(chip, "radeon") ||
        contains_ignore_case(chip, "i915") ||
        contains_ignore_case(chip, "xe") ||
        contains_ignore_case(label, "gpu") ||
        contains_ignore_case(label, "vram") ||
        contains_ignore_case(label, "edge") ||
        contains_ignore_case(label, "junction")) {
        return sensor_type::gpu;
    }

    // Storage hints
    if (contains_ignore_case(chip, "nvme") ||
        contains_ignore_case(chip, "drivetemp") ||
        contains_ignore_case(label, "drive") ||
        contains_ignore_case(label, "ssd") ||
        contains_ignore_case(label, "hdd") ||
        contains_ignore_case(label, "disk") ||
        ((contains_ignore_case(chip, "nvme") ||
          contains_ignore_case(chip, "drivetemp")) &&
         (contains_ignore_case(label, "composite") ||
          contains_ignore_case(label, "sensor 1") ||
          contains_ignore_case(label, "sensor 2")))) {
        return sensor_type::storage;
    }

    // Power / battery hints
    if (contains_ignore_case(chip, "bat") ||
        contains_ignore_case(chip, "battery") ||
        contains_ignore_case(chip, "adp") ||
        contains_ignore_case(chip, "ucsi") ||
        contains_ignore_case(chip, "charger") ||
        contains_ignore_case(label, "bat") ||
        contains_ignore_case(label, "power")) {
        return sensor_type::power_supply;
    }

    // Motherboard / chipset / Super I/O hints
    if (contains_ignore_case(chip, "nct6") ||
        contains_ignore_case(chip, "it87") ||
        contains_ignore_case(chip, "w836") ||
        contains_ignore_case(chip, "f718") ||
        contains_ignore_case(label, "motherboard") ||
        contains_ignore_case(label, "mobo") ||
        contains_ignore_case(label, "system") ||
        contains_ignore_case(label, "chipset") ||
        contains_ignore_case(label, "pch") ||
        contains_ignore_case(label, "vrm")) {
        return sensor_type::motherboard;
    }

    // Ambient hints
    if (contains_ignore_case(chip, "ambient") ||
        contains_ignore_case(label, "ambient") ||
        contains_ignore_case(label, "intake") ||
        contains_ignore_case(label, "exhaust") ||
        contains_ignore_case(label, "air")) {
        return sensor_type::ambient;
    }

    return sensor_type::other;
}

/// Heuristically classifies a thermal zone based on its type name.
inline ::syscape::sensor::thermal_zone_type classify_thermal_zone(
    std::string_view type_name) noexcept {
    using zone_type = ::syscape::sensor::thermal_zone_type;

    if (contains_ignore_case(type_name, "pkg_temp") ||
        contains_ignore_case(type_name, "cpu") ||
        contains_ignore_case(type_name, "core")) {
        return zone_type::cpu;
    }

    if (contains_ignore_case(type_name, "gpu")) {
        return zone_type::gpu;
    }

    if (contains_ignore_case(type_name, "acpitz")) {
        return zone_type::acpi;
    }

    if (contains_ignore_case(type_name, "soc")) {
        return zone_type::soc;
    }

    if (contains_ignore_case(type_name, "bat") ||
        contains_ignore_case(type_name, "bms") ||
        contains_ignore_case(type_name, "pmic") ||
        contains_ignore_case(type_name, "charger")) {
        return zone_type::battery;
    }

    if (contains_ignore_case(type_name, "skin") ||
        contains_ignore_case(type_name, "surface") ||
        contains_ignore_case(type_name, "ambient") ||
        contains_ignore_case(type_name, "chassis")) {
        return zone_type::ambient;
    }

    return zone_type::other;
}

/// Validates that all string fields in a temperature_sensor are well-formed UTF-8.
inline result<void> validate_temperature_sensor(
    const ::syscape::sensor::temperature_sensor& sensor) {
    if (!::syscape::detail::is_valid_utf8(sensor.label)) {
        return fail(errc::invalid_encoding);
    }
    if (sensor.chip_name && !::syscape::detail::is_valid_utf8(*sensor.chip_name)) {
        return fail(errc::invalid_encoding);
    }
    if (sensor.device_id && !::syscape::detail::is_valid_utf8(*sensor.device_id)) {
        return fail(errc::invalid_encoding);
    }
    return {};
}

/// Validates that all string fields in a fan_sensor are well-formed UTF-8.
inline result<void> validate_fan_sensor(
    const ::syscape::sensor::fan_sensor& fan) {
    if (!::syscape::detail::is_valid_utf8(fan.label)) {
        return fail(errc::invalid_encoding);
    }
    if (fan.chip_name && !::syscape::detail::is_valid_utf8(*fan.chip_name)) {
        return fail(errc::invalid_encoding);
    }
    if (fan.device_id && !::syscape::detail::is_valid_utf8(*fan.device_id)) {
        return fail(errc::invalid_encoding);
    }
    return {};
}

/// Validates that all string fields in a thermal_zone are well-formed UTF-8.
inline result<void> validate_thermal_zone(
    const ::syscape::sensor::thermal_zone& zone) {
    if (!::syscape::detail::is_valid_utf8(zone.type_name)) {
        return fail(errc::invalid_encoding);
    }
    if (zone.zone_id && !::syscape::detail::is_valid_utf8(*zone.zone_id)) {
        return fail(errc::invalid_encoding);
    }
    return {};
}

} // namespace sensor_common
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_SENSOR_COMMON_HPP
