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
    const auto t = syscape::sensor::temperatures();
    expect(!t && t.error() == syscape::errc::not_supported,
           "temperatures query must report not_supported on Haiku");
}

} // namespace

int main() {
    test_sensor_queries();
    return failures == 0 ? 0 : 1;
}
