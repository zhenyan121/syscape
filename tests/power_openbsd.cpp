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
    const auto bats = syscape::power::batteries();
    expect(bats.has_value() || bats.error() == syscape::errc::not_supported,
           "batteries query must return list or not_supported");

    const auto ext = syscape::power::external_power_online();
    expect(ext.has_value() || ext.error() == syscape::errc::not_supported,
           "external power query must succeed or report not_supported");
}

} // namespace

int main() {
    test_power_queries();
    return failures == 0 ? 0 : 1;
}
