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
    expect(bats.has_value() || bats.error() == syscape::errc::not_supported ||
               bats.error() == syscape::errc::permission_denied,
           "batteries query must succeed or report expected error");

    const auto sources = syscape::power::power_sources();
    expect(sources.has_value() ||
               sources.error() == syscape::errc::not_supported ||
               sources.error() == syscape::errc::permission_denied,
           "power sources query must succeed or report expected error");
}

} // namespace

int main() {
    test_power_queries();
    return failures == 0 ? 0 : 1;
}
