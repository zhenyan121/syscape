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
    const auto hyp = syscape::virtualization::is_hypervisor_present();
    expect(hyp.has_value() || hyp.error() == syscape::errc::not_supported,
           "hypervisor presence query must succeed or report not_supported");

    const auto vendor = syscape::virtualization::hypervisor();
    expect(vendor.has_value() || vendor.error() == syscape::errc::not_supported,
           "hypervisor vendor query must succeed or report not_supported");

    const auto hyp_name = syscape::virtualization::hypervisor_name();
    expect(hyp_name.has_value() ||
               hyp_name.error() == syscape::errc::not_supported ||
               hyp_name.error() == syscape::errc::not_found,
           "hypervisor name query must succeed or report expected error");

    const auto sandboxed = syscape::virtualization::is_sandboxed();
    expect(sandboxed.has_value() ||
               sandboxed.error() == syscape::errc::not_supported,
           "sandboxed query must succeed or report not_supported");

    const auto sb_type = syscape::virtualization::sandbox();
    expect(sb_type.has_value() ||
               sb_type.error() == syscape::errc::not_supported,
           "sandbox type query must succeed or report not_supported");

    const auto cont = syscape::virtualization::is_container();
    expect(cont.has_value() || cont.error() == syscape::errc::not_supported,
           "container query must succeed or report not_supported");

    const auto wsl = syscape::virtualization::is_wsl();
    expect(wsl && *wsl == false, "is_wsl must be false");

    const auto wsl_ver = syscape::virtualization::wsl_version();
    expect(!wsl_ver && wsl_ver.error() == syscape::errc::not_found,
           "wsl_version must return not_found");

    const auto cgroup = syscape::virtualization::cgroup_hierarchy_version();
    expect(cgroup.has_value() || cgroup.error() == syscape::errc::not_supported,
           "cgroup hierarchy query must succeed or report not_supported");
}

} // namespace

int main() {
    test_virtualization_queries();
    return failures == 0 ? 0 : 1;
}
