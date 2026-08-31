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
               aslr_mode.error() == syscape::errc::not_supported,
           "aslr query must succeed or report not_supported");

    const auto tpm_info = syscape::security::tpm();
    expect(!tpm_info && tpm_info.error() == syscape::errc::not_supported,
           "tpm query must report not_supported on DragonFly BSD");
}

} // namespace

int main() {
    test_security_queries();
    return failures == 0 ? 0 : 1;
}
