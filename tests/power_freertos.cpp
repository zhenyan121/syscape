#include <iostream>

#include <syscape/power.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_power_queries() {
    const auto batts = syscape::power::batteries();
    expect(!batts && batts.error() == syscape::errc::not_supported,
           "batteries must report not_supported on FreeRTOS");
}

} // namespace

int main() {
    test_power_queries();
    return failures == 0 ? 0 : 1;
}
