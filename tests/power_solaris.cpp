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
    const auto batteries = syscape::power::batteries();
    expect(!batteries && batteries.error() == syscape::errc::not_supported,
           "batteries query must report not_supported on Solaris");

    const auto sources = syscape::power::power_sources();
    expect(!sources && sources.error() == syscape::errc::not_supported,
           "power sources query must report not_supported on Solaris");
}

} // namespace

int main() {
    test_power_queries();
    return failures == 0 ? 0 : 1;
}
