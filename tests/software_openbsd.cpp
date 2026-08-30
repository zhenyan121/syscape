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
    expect(svcs.has_value() ||
               svcs.error() == syscape::errc::permission_denied ||
               svcs.error() == syscape::errc::not_supported,
           "services query must return list, permission_denied, or "
           "not_supported");

    const auto pkgs = syscape::software::installed_packages();
    expect(pkgs.has_value() ||
               pkgs.error() == syscape::errc::permission_denied ||
               pkgs.error() == syscape::errc::not_supported,
           "installed_packages query must return list, permission_denied, or "
           "not_supported");
}

} // namespace

int main() {
    test_software_queries();
    return failures == 0 ? 0 : 1;
}
