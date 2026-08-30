#include <iostream>
#include <syscape/sensor.hpp>
#include <syscape/detail/sensor/macos.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_macos_sensor_backend() {
    const auto temps = syscape::sensor::temperatures();
    expect(!temps && temps.error() == syscape::errc::not_supported,
           "Temperatures must report not_supported on unprivileged macOS");

    const auto fans = syscape::sensor::fans();
    expect(!fans && fans.error() == syscape::errc::not_supported,
           "Fans must report not_supported on unprivileged macOS");

    const auto zones = syscape::sensor::thermal_zones();
    expect(!zones && zones.error() == syscape::errc::not_supported,
           "Thermal zones must report not_supported on unprivileged macOS");
}

} // namespace

int main() {
    test_macos_sensor_backend();
    return failures == 0 ? 0 : 1;
}
