#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

#include <syscape/sensor.hpp>
#include <syscape/detail/sensor/common.hpp>
#include <syscape/detail/sensor/linux.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::filesystem::path make_fixture_path(const char* name) {
    return std::filesystem::temp_directory_path() /
           (std::string("syscape-sensor-") + name + "-" +
            std::to_string(static_cast<long long>(::getpid())));
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

void test_unit_conversions_and_classification() {
    namespace scomm = syscape::detail::sensor_common;
    using syscape::sensor::temperature_sensor_type;
    using syscape::sensor::thermal_zone_type;

    // Millicelsius to Celsius conversion
    expect(std::abs(scomm::millicelsius_to_celsius(45000) - 45.0) < 0.001,
           "45000 millicelsius must be 45.0 C");
    expect(std::abs(scomm::millicelsius_to_celsius(0) - 0.0) < 0.001,
           "0 millicelsius must be 0.0 C");
    expect(std::abs(scomm::millicelsius_to_celsius(-12500) - (-12.5)) < 0.001,
           "-12500 millicelsius must be -12.5 C");
    expect(std::abs(scomm::millicelsius_to_celsius(98765) - 98.765) < 0.001,
           "98765 millicelsius must be 98.765 C");

    // 3000 tenths of Kelvin = 300.0 K = 26.85 C.
    expect(std::abs(scomm::tenths_kelvin_to_celsius(3000) - 26.85) < 0.001,
           "3000 tenths of Kelvin must be 26.85 C");
    expect(std::abs(scomm::tenths_kelvin_to_celsius(2732) - 0.05) < 0.001,
           "2732 tenths of Kelvin must be 0.05 C");

    // Temperature sensor classification
    expect(scomm::classify_temperature_sensor("coretemp", "Package id 0") == temperature_sensor_type::cpu,
           "coretemp must classify as CPU");
    expect(scomm::classify_temperature_sensor("k10temp", "Tctl") == temperature_sensor_type::cpu,
           "k10temp Tctl must classify as CPU");
    expect(scomm::classify_temperature_sensor("amdgpu", "edge") == temperature_sensor_type::gpu,
           "amdgpu edge must classify as GPU");
    expect(scomm::classify_temperature_sensor("nouveau", "temp1") == temperature_sensor_type::gpu,
           "nouveau must classify as GPU");
    expect(scomm::classify_temperature_sensor("nvme", "Composite") == temperature_sensor_type::storage,
           "nvme Composite must classify as storage");
    expect(scomm::classify_temperature_sensor("drivetemp", "temp1") == temperature_sensor_type::storage,
           "drivetemp must classify as storage");
    expect(scomm::classify_temperature_sensor("", "SSD Temperature") == temperature_sensor_type::storage,
           "an explicit SSD label must classify as storage");
    expect(scomm::classify_temperature_sensor("BAT0", "temp1") == temperature_sensor_type::power_supply,
           "BAT0 must classify as power supply");
    expect(scomm::classify_temperature_sensor("nct6775", "SYSTIN") == temperature_sensor_type::motherboard,
           "nct6775 must classify as motherboard");
    expect(scomm::classify_temperature_sensor("ambient", "air") == temperature_sensor_type::ambient,
           "ambient air must classify as ambient");
    expect(scomm::classify_temperature_sensor("custom_chip", "temp1") == temperature_sensor_type::other,
           "unrecognized chip must classify as other");

    // Thermal zone classification
    expect(scomm::classify_thermal_zone("x86_pkg_temp") == thermal_zone_type::cpu,
           "x86_pkg_temp must classify as CPU");
    expect(scomm::classify_thermal_zone("cpu-thermal") == thermal_zone_type::cpu,
           "cpu-thermal must classify as CPU");
    expect(scomm::classify_thermal_zone("gpu-thermal") == thermal_zone_type::gpu,
           "gpu-thermal must classify as GPU");
    expect(scomm::classify_thermal_zone("acpitz") == thermal_zone_type::acpi,
           "acpitz must classify as ACPI");
    expect(scomm::classify_thermal_zone("soc_thermal") == thermal_zone_type::soc,
           "soc_thermal must classify as SoC");
    expect(scomm::classify_thermal_zone("battery") == thermal_zone_type::battery,
           "battery must classify as battery");
    expect(scomm::classify_thermal_zone("skin_temp") == thermal_zone_type::ambient,
           "skin_temp must classify as ambient");
    expect(scomm::classify_thermal_zone("custom_zone") == thermal_zone_type::other,
           "custom_zone must classify as other");
}

void test_parsing_helpers() {
    namespace sbackend = syscape::detail::sensor_backend;

    // parse_millicelsius
    const auto m1 = sbackend::parse_millicelsius("45000\n");
    expect(m1.has_value() && *m1 == 45000, "parse_millicelsius 45000\\n must succeed");

    const auto m2 = sbackend::parse_millicelsius(" -15250 \r\n");
    expect(m2.has_value() && *m2 == -15250, "parse_millicelsius -15250 must succeed");

    const auto m3 = sbackend::parse_millicelsius("0");
    expect(m3.has_value() && *m3 == 0, "parse_millicelsius 0 must succeed");

    const auto m_bad = sbackend::parse_millicelsius("abc");
    expect(!m_bad && m_bad.error() == syscape::errc::malformed_data,
           "parse_millicelsius non-numeric must fail with malformed_data");

    const auto m_empty = sbackend::parse_millicelsius("  ");
    expect(!m_empty && m_empty.error() == syscape::errc::malformed_data,
           "parse_millicelsius empty must fail with malformed_data");

    // parse_rpm
    const auto r1 = sbackend::parse_rpm("1800\n");
    expect(r1.has_value() && *r1 == 1800U, "parse_rpm 1800\\n must succeed");

    const auto r2 = sbackend::parse_rpm("0");
    expect(r2.has_value() && *r2 == 0U, "parse_rpm 0 must succeed");

    const auto r_bad = sbackend::parse_rpm("-50");
    expect(!r_bad && r_bad.error() == syscape::errc::malformed_data,
           "parse_rpm negative must fail with malformed_data");
}

void test_synthetic_hwmon_and_thermal() {
    namespace sbackend = syscape::detail::sensor_backend;
    using syscape::sensor::temperature_sensor_type;

    const auto root = make_fixture_path("synth");
    std::filesystem::remove_all(root);

    const auto hwmon_dir = root / "hwmon";
    const auto thermal_dir = root / "thermal";

    // Create hwmon0: k10temp (CPU)
    const auto hw0 = hwmon_dir / "hwmon0";
    write_file(hw0 / "name", "k10temp\n");
    write_file(hw0 / "temp1_input", "48125\n");
    write_file(hw0 / "temp1_label", "Tctl\n");
    write_file(hw0 / "temp1_max", "95000\n");
    write_file(hw0 / "temp1_crit", "105000\n");
    write_file(hw0 / "temp3_input", "45000\n");
    write_file(hw0 / "temp3_label", "Tccd1\n");

    // Create hwmon1: yogafan (Fans)
    const auto hw1 = hwmon_dir / "hwmon1";
    write_file(hw1 / "name", "yogafan\n");
    write_file(hw1 / "fan1_input", "2400\n");
    write_file(hw1 / "fan1_label", "CPU Fan\n");
    write_file(hw1 / "fan1_min", "500\n");
    write_file(hw1 / "fan1_max", "5000\n");
    write_file(hw1 / "fan1_target", "2500\n");
    write_file(hw1 / "fan2_input", "2200\n");

    // Create hwmon2: nvme (Storage)
    const auto hw2 = hwmon_dir / "hwmon2";
    write_file(hw2 / "name", "nvme\n");
    write_file(hw2 / "temp1_input", "36850\n");
    write_file(hw2 / "temp1_label", "Composite\n");
    write_file(hw2 / "temp1_crit", "85000\n");

    // Create hwmon3 with the legacy nested device/ attribute layout.
    const auto hw3 = hwmon_dir / "hwmon3";
    write_file(hw3 / "name", "legacychip\n");
    write_file(hw3 / "device/temp1_input", "41000\n");
    write_file(hw3 / "device/temp1_label", "Drive Bay\n");
    write_file(hw3 / "device/temp2_input", "-12500\n");
    write_file(hw3 / "device/temp2_label", "Cold Storage\n");
    write_file(hw3 / "device/fan1_input", "1750\n");
    write_file(hw3 / "device/fan1_label", "Rear Fan\n");

    // Create hwmon4 with both layouts. Direct attributes must win without
    // returning duplicate device/ probes.
    const auto hw4 = hwmon_dir / "hwmon4";
    write_file(hw4 / "name", "dual-layout\n");
    write_file(hw4 / "temp1_input", "42000\n");
    write_file(hw4 / "temp1_label", "Direct Temperature\n");
    write_file(hw4 / "fan1_input", "1900\n");
    write_file(hw4 / "fan1_label", "Direct Fan\n");
    write_file(hw4 / "device/temp1_input", "43000\n");
    write_file(hw4 / "device/temp1_label", "Duplicate Temperature\n");
    write_file(hw4 / "device/fan1_input", "2000\n");
    write_file(hw4 / "device/fan1_label", "Duplicate Fan\n");

    // Create thermal_zone0: CPU thermal zone with trip points
    const auto tz0 = thermal_dir / "thermal_zone0";
    write_file(tz0 / "type", "x86_pkg_temp\n");
    write_file(tz0 / "temp", "52000\n");
    write_file(tz0 / "mode", "enabled\n");
    write_file(tz0 / "trip_point_0_type", "passive\n");
    write_file(tz0 / "trip_point_0_temp", "90000\n");
    write_file(tz0 / "trip_point_1_type", "critical\n");
    write_file(tz0 / "trip_point_1_temp", "105000\n");

    // Create thermal_zone1: ACPI thermal zone
    const auto tz1 = thermal_dir / "thermal_zone1";
    write_file(tz1 / "type", "acpitz\n");
    write_file(tz1 / "temp", "39000\n");
    write_file(tz1 / "mode", "disabled\n");

    // Query synthetic temperatures
    const auto temps_res = sbackend::temperatures_at(hwmon_dir.string());
    expect(temps_res.has_value(), "Synthetic temperatures query must succeed");
    if (temps_res) {
        expect(temps_res->size() == 6U, "Expected exactly 6 temperature sensors in fixture");
        bool found_tctl = false;
        bool found_tccd1 = false;
        bool found_composite = false;
        bool found_nested = false;
        bool found_negative = false;
        bool found_direct = false;
        bool found_duplicate = false;
        for (const auto& s : *temps_res) {
            if (s.label == "Tctl") {
                found_tctl = true;
                expect(s.type == temperature_sensor_type::cpu, "Tctl type must be CPU");
                expect(std::abs(s.current_celsius - 48.125) < 0.001, "Tctl temp must be 48.125");
                expect(s.max_celsius.has_value() && std::abs(*s.max_celsius - 95.0) < 0.001, "Tctl max must be 95.0");
                expect(s.critical_celsius.has_value() && std::abs(*s.critical_celsius - 105.0) < 0.001, "Tctl crit must be 105.0");
                expect(s.chip_name.has_value() && *s.chip_name == "k10temp", "Tctl chip must be k10temp");
                expect(s.device_id.has_value() && *s.device_id == "hwmon0", "Tctl device_id must be hwmon0");
            } else if (s.label == "Tccd1") {
                found_tccd1 = true;
                expect(std::abs(s.current_celsius - 45.0) < 0.001, "Tccd1 temp must be 45.0");
            } else if (s.label == "Composite") {
                found_composite = true;
                expect(s.type == temperature_sensor_type::storage, "Composite type must be storage");
                expect(std::abs(s.current_celsius - 36.85) < 0.001, "Composite temp must be 36.85");
            } else if (s.label == "Drive Bay") {
                found_nested = true;
                expect(s.type == temperature_sensor_type::storage,
                       "nested Drive Bay sensor must classify as storage");
            } else if (s.label == "Cold Storage") {
                found_negative = true;
                expect(std::abs(s.current_celsius - (-12.5)) < 0.001,
                       "Negative temperature must be preserved");
            } else if (s.label == "Direct Temperature") {
                found_direct = true;
            } else if (s.label == "Duplicate Temperature") {
                found_duplicate = true;
            }
        }
        expect(found_tctl, "Tctl sensor must be present");
        expect(found_tccd1, "Tccd1 sensor must be present");
        expect(found_composite, "Composite sensor must be present");
        expect(found_nested, "Nested device/ temperature must be present");
        expect(found_negative, "Negative nested temperature must be present");
        expect(found_direct, "Direct-layout temperature must be present");
        expect(!found_duplicate, "Nested duplicate temperature must be suppressed");
    }

    // Query synthetic fans
    const auto fans_res = sbackend::fans_at(hwmon_dir.string());
    expect(fans_res.has_value(), "Synthetic fans query must succeed");
    if (fans_res) {
        expect(fans_res->size() == 4U, "Expected exactly 4 fans in fixture");
        bool found_cpu_fan = false;
        bool found_fan2 = false;
        bool found_nested = false;
        bool found_direct = false;
        bool found_duplicate = false;
        for (const auto& f : *fans_res) {
            if (f.label == "CPU Fan") {
                found_cpu_fan = true;
                expect(f.current_rpm == 2400U, "CPU fan RPM must be 2400");
                expect(f.min_rpm.has_value() && *f.min_rpm == 500U, "CPU fan min RPM must be 500");
                expect(f.max_rpm.has_value() && *f.max_rpm == 5000U, "CPU fan max RPM must be 5000");
                expect(f.target_rpm.has_value() && *f.target_rpm == 2500U, "CPU fan target RPM must be 2500");
                expect(f.chip_name.has_value() && *f.chip_name == "yogafan", "CPU fan chip must be yogafan");
            } else if (f.label == "yogafan fan2") {
                found_fan2 = true;
                expect(f.current_rpm == 2200U, "Fan 2 RPM must be 2200");
            } else if (f.label == "Rear Fan") {
                found_nested = true;
                expect(f.current_rpm == 1750U, "Nested rear fan RPM must be 1750");
            } else if (f.label == "Direct Fan") {
                found_direct = true;
            } else if (f.label == "Duplicate Fan") {
                found_duplicate = true;
            }
        }
        expect(found_cpu_fan, "CPU Fan must be present");
        expect(found_fan2, "Fan 2 must be present");
        expect(found_nested, "Nested device/ fan must be present");
        expect(found_direct, "Direct-layout fan must be present");
        expect(!found_duplicate, "Nested duplicate fan must be suppressed");
    }

    // Query synthetic thermal zones
    const auto zones_res = sbackend::thermal_zones_at(thermal_dir.string());
    expect(zones_res.has_value(), "Synthetic thermal zones query must succeed");
    if (zones_res) {
        expect(zones_res->size() == 2U, "Expected exactly 2 thermal zones in fixture");
        bool found_pkg = false;
        bool found_acpi = false;
        for (const auto& z : *zones_res) {
            if (z.type_name == "x86_pkg_temp") {
                found_pkg = true;
                expect(z.type == syscape::sensor::thermal_zone_type::cpu, "x86_pkg_temp type must be CPU");
                expect(std::abs(z.current_celsius - 52.0) < 0.001, "x86_pkg_temp current temp must be 52.0");
                expect(z.passive_celsius.has_value() && std::abs(*z.passive_celsius - 90.0) < 0.001, "passive trip must be 90.0");
                expect(z.critical_celsius.has_value() && std::abs(*z.critical_celsius - 105.0) < 0.001, "critical trip must be 105.0");
                expect(z.enabled, "x86_pkg_temp must be enabled");
            } else if (z.type_name == "acpitz") {
                found_acpi = true;
                expect(z.type == syscape::sensor::thermal_zone_type::acpi, "acpitz type must be ACPI");
                expect(std::abs(z.current_celsius - 39.0) < 0.001, "acpitz current temp must be 39.0");
                expect(!z.enabled, "acpitz must be disabled");
            }
        }
        expect(found_pkg, "x86_pkg_temp zone must be present");
        expect(found_acpi, "acpitz zone must be present");
    }

    // Malformed optional attributes must not masquerade as absent values.
    write_file(hw0 / "temp1_max", "invalid\n");
    const auto malformed_max = sbackend::temperatures_at(hwmon_dir.string());
    expect(!malformed_max && malformed_max.error() == syscape::errc::malformed_data,
           "Malformed optional temperature maximum must propagate");
    write_file(hw0 / "temp1_max", "95000\n");

    write_file(hw1 / "fan1_target", "invalid\n");
    const auto malformed_target = sbackend::fans_at(hwmon_dir.string());
    expect(!malformed_target && malformed_target.error() == syscape::errc::malformed_data,
           "Malformed optional fan target must propagate");
    write_file(hw1 / "fan1_target", "2500\n");

    write_file(tz0 / "mode", "mystery\n");
    const auto malformed_mode = sbackend::thermal_zones_at(thermal_dir.string());
    expect(!malformed_mode && malformed_mode.error() == syscape::errc::malformed_data,
           "Unknown thermal-zone mode must report malformed_data");
    write_file(tz0 / "mode", "enabled\n");

    write_file(tz0 / "trip_point_0_temp", "invalid\n");
    const auto malformed_trip = sbackend::thermal_zones_at(thermal_dir.string());
    expect(!malformed_trip && malformed_trip.error() == syscape::errc::malformed_data,
           "Malformed trip-point temperature must propagate");
    write_file(tz0 / "trip_point_0_temp", "90000\n");

    // Invalid public text and native read failures must propagate.
    write_file(hw0 / "temp1_label", std::string(1U, static_cast<char>(0xFF)));
    const auto invalid_utf8 = sbackend::temperatures_at(hwmon_dir.string());
    expect(!invalid_utf8 && invalid_utf8.error() == syscape::errc::invalid_encoding,
           "Invalid UTF-8 sensor labels must be rejected");
    write_file(hw0 / "temp1_label", "Tctl\n");

    std::filesystem::create_directory(hw0 / "temp4_input");
    const auto native_failure = sbackend::temperatures_at(hwmon_dir.string());
    expect(!native_failure &&
               native_failure.error() == std::errc::is_a_directory,
           "Native sensor input read failures must propagate");
    std::filesystem::remove(hw0 / "temp4_input");

    // Test malformed required temperature data.
    write_file(hw0 / "temp1_input", "corrupted_number\n");
    const auto malformed_res = sbackend::temperatures_at(hwmon_dir.string());
    expect(!malformed_res && malformed_res.error() == syscape::errc::malformed_data,
           "Corrupted temp1_input must report malformed_data");

    // Cleanup fixture
    std::filesystem::remove_all(root);
}

void test_live_linux_sensor_queries() {
    const auto temps = syscape::sensor::temperatures();
    if (temps) {
        std::cout << "Discovered " << temps->size() << " temperature sensors:\n";
        for (const auto& s : *temps) {
            std::cout << "  - [" << s.label << "] " << s.current_celsius << " C"
                      << (s.chip_name ? " (chip: " + *s.chip_name + ")" : "")
                      << (s.device_id ? " (dev: " + *s.device_id + ")" : "")
                      << '\n';
            expect(!s.label.empty(), "Sensor label must not be empty");
        }
    } else {
        std::cout << "Temperatures query returned error: " << temps.error().message() << '\n';
    }

    const auto fans = syscape::sensor::fans();
    if (fans) {
        std::cout << "Discovered " << fans->size() << " fan sensors:\n";
        for (const auto& f : *fans) {
            std::cout << "  - [" << f.label << "] " << f.current_rpm << " RPM"
                      << (f.chip_name ? " (chip: " + *f.chip_name + ")" : "")
                      << '\n';
            expect(!f.label.empty(), "Fan label must not be empty");
        }
    } else {
        std::cout << "Fans query returned error: " << fans.error().message() << '\n';
    }

    const auto zones = syscape::sensor::thermal_zones();
    if (zones) {
        std::cout << "Discovered " << zones->size() << " thermal zones:\n";
        for (const auto& z : *zones) {
            std::cout << "  - [" << z.type_name << "] " << z.current_celsius << " C"
                      << (z.enabled ? " (enabled)" : " (disabled)")
                      << '\n';
            expect(!z.type_name.empty(), "Thermal zone type name must not be empty");
        }
    } else {
        std::cout << "Thermal zones query returned error: " << zones.error().message() << '\n';
    }
}

} // namespace

int main() {
    test_unit_conversions_and_classification();
    test_parsing_helpers();
    test_synthetic_hwmon_and_thermal();
    test_live_linux_sensor_queries();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "All sensor Linux tests passed!\n";
    return 0;
}
