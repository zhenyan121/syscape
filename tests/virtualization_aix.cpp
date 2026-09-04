#include <iostream>

#include <syscape/virtualization.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_virtualization_queries() {
    const auto hv = syscape::virtualization::is_hypervisor_present();
    expect(hv.has_value() || hv.error() == syscape::errc::not_supported,
           "is_hypervisor_present must succeed or report not_supported");

    const auto vendor = syscape::virtualization::hypervisor();
    expect(vendor.has_value() || vendor.error() == syscape::errc::not_supported,
           "hypervisor vendor query must succeed or report not_supported");

    const auto container = syscape::virtualization::is_container();
    expect(container.has_value() ||
               container.error() == syscape::errc::not_supported,
           "is_container query must succeed or report not_supported");

    const auto wsl = syscape::virtualization::is_wsl();
    expect(wsl.has_value() && !*wsl, "is_wsl must be false on AIX");

    const auto sandboxed = syscape::virtualization::is_sandboxed();
    expect(sandboxed.has_value() ||
               sandboxed.error() == syscape::errc::not_supported,
           "is_sandboxed query must succeed or report not_supported");
}

} // namespace

int main() {
    test_virtualization_queries();
    return failures == 0 ? 0 : 1;
}
