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

    const auto hv = syscape::virtualization::hypervisor();
    expect(hv.has_value(), "hypervisor classification query must succeed");

    const auto hv_name = syscape::virtualization::hypervisor_name();
    expect(hv_name.has_value() || hv_name.error() == syscape::errc::not_found,
           "hypervisor name must succeed or report not_found");

    const auto container = syscape::virtualization::container();
    expect(container.has_value(),
           "container classification query must succeed");

    const auto container_name = syscape::virtualization::container_name();
    expect(container_name.has_value() ||
               container_name.error() == syscape::errc::not_found,
           "container name must succeed or report not_found");

    const auto wsl = syscape::virtualization::is_wsl();
    expect(wsl && !*wsl, "WSL presence must be false on DragonFly BSD");

    const auto wsl_version = syscape::virtualization::wsl_version();
    expect(!wsl_version && wsl_version.error() == syscape::errc::not_found,
           "WSL version must report not_found on DragonFly BSD");

    const auto sandboxed = syscape::virtualization::is_sandboxed();
    expect(sandboxed.has_value(), "sandbox presence query must succeed");

    const auto sandbox = syscape::virtualization::sandbox();
    expect(sandbox.has_value(), "sandbox classification query must succeed");

    const auto cgroup = syscape::virtualization::current_cgroup();
    expect(!cgroup && cgroup.error() == syscape::errc::not_supported,
           "cgroup query must report not_supported on DragonFly BSD");

    const auto namespaces = syscape::virtualization::namespaces();
    expect(!namespaces && namespaces.error() == syscape::errc::not_supported,
           "namespace query must report not_supported on DragonFly BSD");

    const auto isolated = syscape::virtualization::is_namespace_isolated();
    expect(!isolated && isolated.error() == syscape::errc::not_supported,
           "namespace isolation must report not_supported on DragonFly BSD");
}

} // namespace

int main() {
    test_virtualization_queries();
    return failures == 0 ? 0 : 1;
}
