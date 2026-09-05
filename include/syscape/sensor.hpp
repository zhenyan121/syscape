#ifndef SYSCAPE_SENSOR_HPP
#define SYSCAPE_SENSOR_HPP

/// @file
/// @brief Hosted hardware sensors, thermal zones, temperatures, and fan speeds.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms, Android, and OpenHarmony).
/// @note Apple mobile platforms expose no permitted public temperature, fan,
/// or thermal-zone inventory source to this C++ interface, so all queries
/// report not_supported.
/// @note This module exposes:
/// - Hardware temperature sensors from hwmon, ACPI, or platform monitoring
/// (temperatures()).
/// - Hardware fan speed sensors in RPM (fans()).
/// - Operating-system thermal zones and cooling trip points (thermal_zones()).
/// @note Linux queries /sys/class/hwmon and /sys/class/thermal.
/// Android queries /sys/class/thermal for thermal zones and temperature
/// sensors. Windows, macOS, AIX, and HP-UX currently report not_supported
/// because no stable, public backend has been implemented for these queries.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/sensor.hpp requires C++17 or later"
#endif

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace syscape {
namespace sensor {

/// Classification of a temperature sensor based on device origin or purpose.
enum class temperature_sensor_type : std::uint8_t {
    /// The sensor origin is unclassified or unknown.
    unknown,
    /// CPU core, package, or socket temperature.
    cpu,
    /// GPU core, memory junction, or board temperature.
    gpu,
    /// Storage drive (NVMe, SSD, HDD) temperature.
    storage,
    /// Motherboard, chipset, or VRM temperature.
    motherboard,
    /// Ambient, chassis, or intake air temperature.
    ambient,
    /// Battery, power supply, or charger temperature.
    power_supply,
    /// Other device or sensor type.
    other
};

/// Classification of an operating-system thermal zone.
enum class thermal_zone_type : std::uint8_t {
    /// The thermal zone type is unclassified or unknown.
    unknown,
    /// CPU thermal zone.
    cpu,
    /// GPU thermal zone.
    gpu,
    /// ACPI motherboard/chassis thermal zone.
    acpi,
    /// SoC thermal zone.
    soc,
    /// Battery or power management thermal zone.
    battery,
    /// Skin, surface, or ambient thermal zone.
    ambient,
    /// Other thermal zone.
    other
};

/// Information describing a single hardware temperature sensor reading.
struct temperature_sensor {
    /// Human-readable label or description (e.g. "Tctl", "Package id 0", "Composite").
    std::string label;

    /// Sensor classification enum.
    temperature_sensor_type type = temperature_sensor_type::unknown;

    /// Current temperature measurement in degrees Celsius (°C).
    double current_celsius = 0.0;

    /// Optional high warning threshold in degrees Celsius (°C).
    std::optional<double> max_celsius;

    /// Optional critical shutdown threshold in degrees Celsius (°C).
    std::optional<double> critical_celsius;

    /// Underlying hardware driver or chip name (e.g. "k10temp", "coretemp", "nvme"), if exposed.
    std::optional<std::string> chip_name;

    /// Device or sysfs identifier (e.g. "hwmon0", "thermal_zone0"), if exposed.
    std::optional<std::string> device_id;
};

/// Information describing a single hardware fan sensor reading.
struct fan_sensor {
    /// Human-readable label or description (e.g. "fan1", "CPU Fan").
    std::string label;

    /// Current rotational speed in revolutions per minute (RPM).
    std::uint32_t current_rpm = 0U;

    /// Optional minimum operational speed in RPM.
    std::optional<std::uint32_t> min_rpm;

    /// Optional maximum operational speed in RPM.
    std::optional<std::uint32_t> max_rpm;

    /// Optional target or commanded speed in RPM.
    std::optional<std::uint32_t> target_rpm;

    /// Underlying hardware driver or chip name (e.g. "yogafan", "nct6775"), if exposed.
    std::optional<std::string> chip_name;

    /// Device or sysfs identifier (e.g. "hwmon1"), if exposed.
    std::optional<std::string> device_id;
};

/// Information describing an operating-system thermal zone.
struct thermal_zone {
    /// Verbatim thermal zone type name (e.g. "x86_pkg_temp", "acpitz", "cpu-thermal").
    std::string type_name;

    /// Thermal zone classification enum.
    thermal_zone_type type = thermal_zone_type::unknown;

    /// Current temperature in degrees Celsius (°C).
    double current_celsius = 0.0;

    /// Optional passive cooling trip point in degrees Celsius (°C).
    std::optional<double> passive_celsius;

    /// Optional critical shutdown trip point in degrees Celsius (°C).
    std::optional<double> critical_celsius;

    /// Thermal zone system identifier (e.g. "thermal_zone0"), if exposed.
    std::optional<std::string> zone_id;

    /// Whether the thermal zone is enabled and active.
    bool enabled = true;
};

} // namespace sensor
} // namespace syscape

#include <syscape/detail/sensor/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__) && !defined(SYSCAPE_TARGET_OPENHARMONY) &&           \
    !defined(SYSCAPE_TARGET_AIX) && !defined(SYSCAPE_TARGET_HPUX)
#include <syscape/detail/sensor/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/sensor/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/sensor/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/sensor/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/sensor/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/sensor/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/sensor/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/sensor/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/sensor/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/sensor/openharmony.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/sensor/solaris.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__HAIKU__)
#include <syscape/detail/sensor/haiku.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_AIX)
#include <syscape/detail/sensor/aix.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_HPUX)
#include <syscape/detail/sensor/hpux.hpp>
#else
#include <syscape/detail/sensor/generic.hpp>
#endif

namespace syscape {
namespace sensor {

/// Enumerates hardware temperature sensors on the system.
///
/// @return A list of temperature_sensor entries; not_supported when hardware
/// monitoring is unavailable on the platform; permission_denied when access
/// is denied; malformed_data for invalid platform values; invalid_encoding if
/// platform text is invalid UTF-8; or a native I/O error.
inline result<std::vector<temperature_sensor>> temperatures() {
    return detail::sensor_backend::temperatures();
}

/// Enumerates hardware fan sensors and rotational speeds on the system.
///
/// @return A list of fan_sensor entries in RPM; not_supported when fan monitoring
/// is unavailable on the platform; permission_denied when access is denied;
/// malformed_data for invalid platform values; invalid_encoding if platform
/// text is invalid UTF-8; or a native I/O error.
inline result<std::vector<fan_sensor>> fans() {
    return detail::sensor_backend::fans();
}

/// Enumerates operating-system thermal zones and trip points on the system.
///
/// @return A list of thermal_zone entries; not_supported when thermal zones are
/// unavailable on the platform; permission_denied when access is denied;
/// malformed_data for invalid platform values; invalid_encoding if platform
/// text is invalid UTF-8; or a native I/O error.
inline result<std::vector<thermal_zone>> thermal_zones() {
    return detail::sensor_backend::thermal_zones();
}

} // namespace sensor
} // namespace syscape

#endif // SYSCAPE_SENSOR_HPP
