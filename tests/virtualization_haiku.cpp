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
           "is_hypervisor_present query must succeed or report not_supported");
    const auto container = syscape::virtualization::is_container();
    expect(container && !*container, "is_container must be false on Haiku");
    const auto wsl = syscape::virtualization::is_wsl();
    expect(wsl && !*wsl, "is_wsl must be false on Haiku");
    const auto sandboxed = syscape::virtualization::is_sandboxed();
    expect(sandboxed && !*sandboxed, "is_sandboxed must be false on Haiku");
}

} // namespace

int main() {
    test_virtualization_queries();
    return failures == 0 ? 0 : 1;
}
