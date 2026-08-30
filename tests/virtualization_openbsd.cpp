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
    const auto is_hv = syscape::virtualization::is_hypervisor_present();
    expect(is_hv.has_value(), "is_hypervisor_present query must succeed");

    const auto is_cont = syscape::virtualization::is_container();
    expect(is_cont.has_value(), "is_container query must succeed");
}

} // namespace

int main() {
    test_virtualization_queries();
    return failures == 0 ? 0 : 1;
}
