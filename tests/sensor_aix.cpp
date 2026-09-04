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
    expect(!temps && temps.error() == syscape::errc::not_supported,
           "temperatures query must report not_supported on AIX");

    const auto fans = syscape::sensor::fans();
    expect(!fans && fans.error() == syscape::errc::not_supported,
           "fans query must report not_supported on AIX");
}

} // namespace

int main() {
    test_sensor_queries();
    return failures == 0 ? 0 : 1;
}
