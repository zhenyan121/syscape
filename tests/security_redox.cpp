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
    const auto sb = syscape::security::secure_boot();
    expect(sb.error() == syscape::errc::not_supported,
           "secure boot query must report not_supported on Redox OS");

    const auto tpm = syscape::security::tpm();
    expect(tpm.error() == syscape::errc::not_supported,
           "tpm query must report not_supported on Redox OS");

    const auto aslr = syscape::security::aslr();
    expect(aslr.error() == syscape::errc::not_supported,
           "aslr query must report not_supported on Redox OS");

    const auto caps = syscape::security::capabilities();
    expect(caps.error() == syscape::errc::not_supported,
           "capabilities query must report not_supported on Redox OS");
}

} // namespace

int main() {
    test_security_queries();
    return failures == 0 ? 0 : 1;
}
