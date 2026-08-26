#include <cassert>
#include <iostream>
#include <syscape/software.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_macos_software_backend() {
#if defined(__APPLE__) && defined(__MACH__)
    using namespace syscape::software;
    using namespace syscape::detail::software_backend::macos_impl;

    const auto drvs = loaded_drivers();
    expect(!drvs && drvs.error() == syscape::errc::not_supported, "macOS loaded_drivers must report not_supported");

    const auto svcs = services();
    if (svcs) {
        for (std::size_t i = 1; i < svcs->size(); ++i) {
            expect((*svcs)[i - 1].name <= (*svcs)[i].name, "services must be sorted");
        }
    }

    const auto pkgs = installed_packages();
    if (pkgs) {
        for (std::size_t i = 1; i < pkgs->size(); ++i) {
            expect((*pkgs)[i - 1].name <= (*pkgs)[i].name, "packages must be sorted");
        }
    }
#endif
}

} // namespace

int main() {
    test_macos_software_backend();
    return failures;
}
