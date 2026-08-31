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
           "batteries query must succeed or report not_supported");

    const auto sources = syscape::power::power_sources();
    expect(sources.has_value() ||
               sources.error() == syscape::errc::not_supported,
           "power sources query must succeed or report not_supported");

    const auto ext = syscape::power::external_power_online();
    expect(ext.has_value() || ext.error() == syscape::errc::not_supported ||
               ext.error() == syscape::errc::not_found,
           "external power query must succeed, report not_supported, or report "
           "not_found");

    const auto secs = syscape::power::seconds_until_empty();
    expect(secs.has_value() || secs.error() == syscape::errc::not_supported ||
               secs.error() == syscape::errc::not_found,
           "seconds until empty query must succeed, report not_supported, or "
           "report "
           "not_found");
}

} // namespace

int main() {
    test_power_queries();
    return failures == 0 ? 0 : 1;
}
