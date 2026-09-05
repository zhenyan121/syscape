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

void test_sensor_queries() {
    const auto temps = syscape::sensor::temperatures();
    expect(temps.error() == syscape::errc::not_supported,
           "temperatures query must report not_supported on GNU/Hurd");

    const auto fans = syscape::sensor::fans();
    expect(fans.error() == syscape::errc::not_supported,
           "fans query must report not_supported on GNU/Hurd");

    const auto zones = syscape::sensor::thermal_zones();
    expect(zones.error() == syscape::errc::not_supported,
           "thermal zones query must report not_supported on GNU/Hurd");
}

} // namespace

int main() {
    test_sensor_queries();
    return failures == 0 ? 0 : 1;
}
