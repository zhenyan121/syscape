#include <iostream>

#include <syscape/security.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_security_queries() {
    const auto aslr_mode = syscape::security::aslr();
    expect(aslr_mode.has_value() ||
               aslr_mode.error() == syscape::errc::not_supported ||
               aslr_mode.error() == syscape::errc::permission_denied,
           "aslr query must succeed, report not_supported, or report "
           "permission_denied");

    const auto privileges = syscape::security::privileges();
    expect(privileges.has_value() ||
               privileges.error() == syscape::errc::not_supported,
           "privileges must succeed or report not_supported");

    const auto enc_empty = syscape::security::volume_encryption("");
    expect(!enc_empty && enc_empty.error() == syscape::errc::invalid_argument,
           "volume_encryption with empty path must fail with invalid_argument");
}

} // namespace

int main() {
    test_security_queries();
    return failures == 0 ? 0 : 1;
}
