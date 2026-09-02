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
    expect(mfg && *mfg == "Apple", "system manufacturer must be Apple");

    const auto model = syscape::hardware::system_product_name();
    expect(model && !model->empty(), "system model must be nonempty");

    const auto chassis = syscape::hardware::chassis_form_factor();
    expect(chassis.error() == syscape::errc::not_supported,
           "chassis form factor must report not_supported");
}

} // namespace

int main() {
    test_hardware_queries();
    return failures == 0 ? 0 : 1;
}
