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
    const auto sb = syscape::security::is_secure_boot_enabled();
    expect(!sb && sb.error() == syscape::errc::not_supported,
           "is_secure_boot_enabled query must report not_supported on AIX");

    const auto tpm = syscape::security::tpm();
    expect(!tpm && tpm.error() == syscape::errc::not_supported,
           "tpm query must report not_supported on AIX");

    const auto aslr = syscape::security::aslr();
    expect(!aslr && aslr.error() == syscape::errc::not_supported,
           "aslr query must report not_supported on AIX");
}

} // namespace

int main() {
    test_security_queries();
    return failures == 0 ? 0 : 1;
}
