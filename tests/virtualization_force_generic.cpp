#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include <syscape/virtualization.hpp>

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
    expect_unsupported("is_hypervisor_present",
                       syscape::virtualization::is_hypervisor_present);
    expect_unsupported("hypervisor",
                       syscape::virtualization::hypervisor);
    expect_unsupported("hypervisor_name",
                       syscape::virtualization::hypervisor_name);
    expect_unsupported("is_container",
                       syscape::virtualization::is_container);
    expect_unsupported("container",
                       syscape::virtualization::container);
    expect_unsupported("container_name",
                       syscape::virtualization::container_name);
    expect_unsupported("is_wsl",
                       syscape::virtualization::is_wsl);
    expect_unsupported("wsl_version",
                       syscape::virtualization::wsl_version);
    expect_unsupported("is_sandboxed",
                       syscape::virtualization::is_sandboxed);
    expect_unsupported("sandbox",
                       syscape::virtualization::sandbox);
    return failures == 0 ? 0 : 1;
}
