#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include <syscape/security.hpp>
#include <syscape/detail/security/common.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_macos_security_backend() {
    const auto secure_boot = syscape::security::secure_boot();
    expect(!secure_boot &&
               secure_boot.error() == syscape::errc::not_supported,
           "Secure Boot must report not_supported without a stable public API");

    const auto sip = syscape::security::is_sip_enabled();
    expect(!sip && sip.error() == syscape::errc::not_supported,
           "SIP must report not_supported without a stable public API");

    const auto tpm_res = syscape::security::tpm();
    expect(tpm_res.has_value(), "TPM query on macOS must succeed");
    if (tpm_res) {
        expect(!tpm_res->present, "macOS has no standard TPM");
        expect(tpm_res->version == syscape::security::tpm_version::none,
               "macOS TPM version must be none");
    }

    const auto lsm_res = syscape::security::security_modules();
    expect(!lsm_res && lsm_res.error() == syscape::errc::not_supported,
           "LSMs must report not_supported on macOS");

    const auto lock_res = syscape::security::lockdown();
    expect(!lock_res && lock_res.error() == syscape::errc::not_supported,
           "Kernel lockdown must report not_supported on macOS");
}

} // namespace

int main() {
    test_macos_security_backend();
    return failures == 0 ? 0 : 1;
}
