#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include <windows.h>
#include <tbs.h>

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

void test_windows_security_backend() {
    const auto sb_res = syscape::security::secure_boot();
    // In Windows tests, verify that the query either succeeds with a valid state,
    // or fails with not_supported (legacy BIOS) or permission_denied.
    if (sb_res) {
        expect(*sb_res == syscape::security::secure_boot_state::enabled ||
               *sb_res == syscape::security::secure_boot_state::disabled ||
               *sb_res == syscape::security::secure_boot_state::audit,
               "Secure boot state must be valid enum");
    } else {
        expect(sb_res.error() == syscape::errc::not_supported ||
               sb_res.error() == syscape::errc::permission_denied,
               "Failure must be not_supported or permission_denied");
    }

    const auto tpm_res = syscape::security::tpm();
    if (tpm_res) {
        if (tpm_res->present) {
            expect(tpm_res->version != syscape::security::tpm_version::none,
                   "Present TPM must not have version none");
        } else {
            expect(tpm_res->version == syscape::security::tpm_version::none,
                   "Absent TPM must report version none");
        }
    } else {
        expect(static_cast<bool>(tpm_res.error()),
               "A failed TPM query must carry an error");
    }

    const auto aslr_res = syscape::security::aslr();
    if (aslr_res) {
        expect(*aslr_res == syscape::security::aslr_mode::disabled ||
               *aslr_res == syscape::security::aslr_mode::partial ||
               *aslr_res == syscape::security::aslr_mode::full,
               "ASLR mode must be a valid enum");
    }

    const auto vuln_res = syscape::security::cpu_vulnerabilities();
    expect(!vuln_res && vuln_res.error() == syscape::errc::not_supported,
           "cpu_vulnerabilities must report not_supported on Windows");

    const auto caps_res = syscape::security::capabilities();
    expect(!caps_res && caps_res.error() == syscape::errc::not_supported,
           "capabilities must report not_supported on Windows");

    const auto privs_res = syscape::security::privileges();
    if (privs_res) {
        for (const auto& priv : *privs_res) {
            expect(!priv.name.empty(), "Privilege name must not be empty");
        }
    } else {
        expect(static_cast<bool>(privs_res.error()),
               "Failed privileges query must report an error");
    }

    const auto enc_res = syscape::security::volume_encryption("C:\\");
    expect(!enc_res && enc_res.error() == syscape::errc::not_supported,
           "Volume encryption must report not_supported on Windows");

    const auto enc_vols = syscape::security::encrypted_volumes();
    expect(!enc_vols && enc_vols.error() == syscape::errc::not_supported,
           "encrypted_volumes must report not_supported on Windows");
}

} // namespace

int main() {
    test_windows_security_backend();
    return failures == 0 ? 0 : 1;
}
