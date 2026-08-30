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
    expect(temps.has_value(), "temperatures query must succeed");

    const auto zones = syscape::sensor::thermal_zones();
    expect(zones.has_value(), "thermal_zones query must succeed");
}

} // namespace

int main() {
    test_sensor_queries();
    return failures == 0 ? 0 : 1;
}
