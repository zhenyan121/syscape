#include <cmath>
#include <iostream>
#include <syscape/detail/sensor/windows.hpp>
#include <syscape/sensor.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_synthetic_acpi_sensor_parsing() {
    using namespace syscape::detail::sensor_backend::windows_impl;

    msacpi_thermal_zone_record valid_rec;
    valid_rec.instance_name = "ACPI\\ThermalZone\\TZ01_0";
    valid_rec.current_temperature_tenths_k = 3000U; // 300.0 K = 26.85 °C
    valid_rec.critical_trip_point_tenths_k = 3732U; // 373.2 K = 100.05 °C
    valid_rec.passive_trip_point_tenths_k = 3532U;  // 353.2 K = 80.05 °C
    valid_rec.active = true;

    const auto tz_res = parse_msacpi_thermal_zone(valid_rec);
    expect(tz_res.has_value(), "Valid ACPI thermal zone must parse successfully");
    if (tz_res.has_value()) {
        expect(std::abs(tz_res->current_celsius - 26.85) < 0.01,
               "3000 tenths of Kelvin must convert to 26.85 C");
        expect(tz_res->critical_celsius.has_value(), "Critical threshold must be present");
        expect(tz_res->passive_celsius.has_value(), "Passive threshold must be present");
        expect(tz_res->zone_id.has_value() && *tz_res->zone_id == "ACPI\\ThermalZone\\TZ01_0",
               "Zone ID must match instance name");
        expect(tz_res->enabled, "Zone must be enabled");
    }

    const auto s_res = parse_msacpi_temperature_sensor(valid_rec);
    expect(s_res.has_value(), "Valid ACPI temperature sensor must parse successfully");
    if (s_res.has_value()) {
        expect(std::abs(s_res->current_celsius - 26.85) < 0.01,
               "Sensor current temperature must be 26.85 C");
        expect(s_res->chip_name.has_value() && *s_res->chip_name == "ACPI",
               "Chip name must be ACPI");
        expect(s_res->type == syscape::sensor::temperature_sensor_type::motherboard ||
               s_res->type == syscape::sensor::temperature_sensor_type::other,
               "Sensor classification must be valid");
    }

    // Zero temperature check (uninitialized sensor failure)
    msacpi_thermal_zone_record zero_rec = valid_rec;
    zero_rec.current_temperature_tenths_k = 0U;
    const auto zero_tz = parse_msacpi_thermal_zone(zero_rec);
    expect(!zero_tz && zero_tz.error() == syscape::errc::malformed_data,
           "0 Kelvin sensor reading must be rejected as malformed data");

    // Unphysical temperature check (> 1000 K)
    msacpi_thermal_zone_record overflow_rec = valid_rec;
    overflow_rec.current_temperature_tenths_k = 15000U;
    const auto overflow_tz = parse_msacpi_thermal_zone(overflow_rec);
    expect(!overflow_tz && overflow_tz.error() == syscape::errc::malformed_data,
           "Unphysical temperature must be rejected as malformed data");
}

void test_windows_sensor_backend() {
    const auto temps = syscape::sensor::temperatures();
    if (temps) {
        for (const auto& sensor : *temps) {
            expect(!sensor.label.empty(), "Sensor label must not be empty");
        }
    } else {
        expect(temps.error() == syscape::errc::not_supported ||
               temps.error() == syscape::errc::permission_denied,
               "Failure must be not_supported or permission_denied");
    }

    const auto fans = syscape::sensor::fans();
    if (fans) {
        for (const auto& fan : *fans) {
            expect(!fan.label.empty(), "Fan label must not be empty");
        }
    } else {
        expect(fans.error() == syscape::errc::not_supported ||
               fans.error() == syscape::errc::permission_denied,
               "Failure must be not_supported or permission_denied");
    }

    const auto zones = syscape::sensor::thermal_zones();
    if (zones) {
        for (const auto& zone : *zones) {
            expect(!zone.type_name.empty(), "Zone type name must not be empty");
        }
    } else {
        expect(zones.error() == syscape::errc::not_supported ||
               zones.error() == syscape::errc::permission_denied,
               "Failure must be not_supported or permission_denied");
    }
}

} // namespace

int main() {
    test_windows_synthetic_acpi_sensor_parsing();
    test_windows_sensor_backend();
    return failures == 0 ? 0 : 1;
}
