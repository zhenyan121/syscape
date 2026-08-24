#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include <syscape/hardware.hpp>

namespace {

int failures = 0;

template <typename Query>
void expect_unsupported(const char* label, const Query& query) {
    const auto value = query();
    if (value || value.error() != std::errc::operation_not_supported) {
        std::cerr << "FAIL: " << label << " must report not_supported\n";
        ++failures;
    }
}

} // namespace

int main() {
    expect_unsupported("system_manufacturer",
                       syscape::hardware::system_manufacturer);
    expect_unsupported("system_product_name",
                       syscape::hardware::system_product_name);
    expect_unsupported("system_product_version",
                       syscape::hardware::system_product_version);
    expect_unsupported("motherboard_manufacturer",
                       syscape::hardware::motherboard_manufacturer);
    expect_unsupported("motherboard_product_name",
                       syscape::hardware::motherboard_product_name);
    expect_unsupported("motherboard_version",
                       syscape::hardware::motherboard_version);
    expect_unsupported("firmware_vendor",
                       syscape::hardware::firmware_vendor);
    expect_unsupported("firmware_version",
                       syscape::hardware::firmware_version);
    expect_unsupported("firmware_release_date",
                       syscape::hardware::firmware_release_date);
    expect_unsupported("chassis_form_factor",
                       syscape::hardware::chassis_form_factor);
    expect_unsupported("hardware_uuid", syscape::hardware::hardware_uuid);
    return failures == 0 ? 0 : 1;
}
