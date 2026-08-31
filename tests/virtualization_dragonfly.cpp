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
    const auto hv_present = syscape::virtualization::is_hypervisor_present();
    expect(hv_present.has_value() ||
               hv_present.error() == syscape::errc::not_supported,
           "hypervisor presence query must succeed or report not_supported");

    const auto is_cont = syscape::virtualization::is_container();
    expect(is_cont.has_value() ||
               is_cont.error() == syscape::errc::not_supported,
           "is_container query must succeed or report not_supported");
}

} // namespace

int main() {
    test_virtualization_queries();
    return failures == 0 ? 0 : 1;
}
