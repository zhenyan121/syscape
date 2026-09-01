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
    const auto srvs = syscape::software::services();
    expect(srvs.has_value() || srvs.error() == syscape::errc::not_supported ||
               srvs.error() == syscape::errc::permission_denied,
           "services query must succeed or report expected error");

    const auto pkgs = syscape::software::installed_packages();
    expect(pkgs.has_value() || pkgs.error() == syscape::errc::not_supported ||
               pkgs.error() == syscape::errc::permission_denied,
           "packages query must succeed or report expected error");
}

} // namespace

int main() {
    test_software_queries();
    return failures == 0 ? 0 : 1;
}
