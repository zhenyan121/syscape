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
    expect(temps.has_value() || temps.error() == syscape::errc::not_supported ||
               temps.error() == syscape::errc::permission_denied,
           "temperatures query must succeed or report expected error");

    const auto fans = syscape::sensor::fans();
    expect(fans.has_value() || fans.error() == syscape::errc::not_supported ||
               fans.error() == syscape::errc::permission_denied,
           "fans query must succeed or report expected error");
}

} // namespace

int main() {
    test_sensor_queries();
    return failures == 0 ? 0 : 1;
}
