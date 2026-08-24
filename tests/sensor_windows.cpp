#include <iostream>
#include <syscape/sensor.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
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
    test_windows_sensor_backend();
    return failures == 0 ? 0 : 1;
}
