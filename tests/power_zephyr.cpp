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
    expect(batts.error() == syscape::errc::not_supported,
           "batteries query must report not_supported on Zephyr");

    const auto sources = syscape::power::power_sources();
    expect(sources.error() == syscape::errc::not_supported,
           "power sources query must report not_supported on Zephyr");

    const auto ext = syscape::power::external_power_online();
    expect(ext.error() == syscape::errc::not_supported,
           "external power query must report not_supported on Zephyr");
}

} // namespace

int main() {
    test_power_queries();
    return failures == 0 ? 0 : 1;
}
