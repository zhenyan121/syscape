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

void test_software_queries() {
    const auto svcs = syscape::software::services();
    expect(!svcs && svcs.error() == syscape::errc::not_supported,
           "services query must report not_supported on GNU/Hurd");

    const auto pkgs = syscape::software::installed_packages();
    expect(!pkgs && pkgs.error() == syscape::errc::not_supported,
           "installed_packages query must report not_supported on GNU/Hurd");
}

} // namespace

int main() {
    test_software_queries();
    return failures == 0 ? 0 : 1;
}
