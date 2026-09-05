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
    const auto bat = syscape::power::batteries();
    expect(bat.error() == syscape::errc::not_supported,
           "batteries query must report not_supported on Redox OS");

    const auto src = syscape::power::power_sources();
    expect(src.error() == syscape::errc::not_supported,
           "power sources query must report not_supported on Redox OS");

    const auto online = syscape::power::external_power_online();
    expect(online.error() == syscape::errc::not_supported,
           "external power query must report not_supported on Redox OS");

    const auto empty = syscape::power::seconds_until_empty();
    expect(empty.error() == syscape::errc::not_supported,
           "seconds until empty query must report not_supported on Redox OS");
}

} // namespace

int main() {
    test_power_queries();
    return failures == 0 ? 0 : 1;
}
