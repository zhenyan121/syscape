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
    expect(!sb && sb.error() == syscape::errc::not_supported,
           "secure_boot must report not_supported on Emscripten");

    const auto tpm = syscape::security::tpm();
    expect(!tpm && tpm.error() == syscape::errc::not_supported,
           "tpm must report not_supported on Emscripten");

    const auto aslr = syscape::security::aslr();
    expect(!aslr && aslr.error() == syscape::errc::not_supported,
           "aslr must report not_supported on Emscripten");
}

} // namespace

int main() {
    test_security_queries();
    return failures == 0 ? 0 : 1;
}
