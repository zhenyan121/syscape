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

    const auto vulnerabilities = syscape::security::cpu_vulnerabilities();
    expect(!vulnerabilities &&
               vulnerabilities.error() == syscape::errc::not_supported,
           "CPU vulnerabilities must report not_supported on DragonFly BSD");

    const auto capabilities = syscape::security::capabilities();
    expect(!capabilities &&
               capabilities.error() == syscape::errc::not_supported,
           "capabilities must report not_supported on DragonFly BSD");

    const auto privileges = syscape::security::privileges();
    expect(!privileges && privileges.error() == syscape::errc::not_supported,
           "privileges must report not_supported on DragonFly BSD");

    const auto root_encryption = syscape::security::volume_encryption("/");
    expect(!root_encryption &&
               root_encryption.error() == syscape::errc::not_supported,
           "volume encryption must report not_supported on DragonFly BSD");

    const auto encrypted = syscape::security::encrypted_volumes();
    expect(!encrypted && encrypted.error() == syscape::errc::not_supported,
           "encrypted volumes must report not_supported on DragonFly BSD");
}

} // namespace

int main() {
    test_security_queries();
    return failures == 0 ? 0 : 1;
}
