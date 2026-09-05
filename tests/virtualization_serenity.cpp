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
    expect(hyp.error() == syscape::errc::not_supported,
           "hypervisor query must report not_supported on SerenityOS");

    const auto cont = syscape::virtualization::is_container();
    expect(cont.error() == syscape::errc::not_supported,
           "container query must report not_supported on SerenityOS");

    const auto wsl = syscape::virtualization::is_wsl();
    expect(wsl.error() == syscape::errc::not_supported,
           "is_wsl must report not_supported on SerenityOS");

    const auto sandbox = syscape::virtualization::is_sandboxed();
    expect(sandbox.error() == syscape::errc::not_supported,
           "is_sandboxed must report not_supported on SerenityOS");
}

} // namespace

int main() {
    test_virtualization_queries();
    return failures == 0 ? 0 : 1;
}
