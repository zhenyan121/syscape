#include <iostream>

#include <syscape/hardware.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_hardware_queries() {
    const auto mfg = syscape::hardware::system_manufacturer();
    expect(mfg || mfg.error() == syscape::errc::not_found ||
               mfg.error() == syscape::errc::not_supported,
           "system manufacturer query must succeed or report expected error");

    const auto prod = syscape::hardware::system_product_name();
    expect(prod || prod.error() == syscape::errc::not_found ||
               prod.error() == syscape::errc::not_supported,
           "system product name query must succeed or report expected error");
}

} // namespace

int main() {
    test_hardware_queries();
    return failures == 0 ? 0 : 1;
}
