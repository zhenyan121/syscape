#include <cassert>
#include <syscape/security.hpp>

int main() {
    const auto sb = syscape::security::secure_boot();
    assert(!sb);
    assert(sb.error() == syscape::errc::not_supported);

    const auto is_sb = syscape::security::is_secure_boot_enabled();
    assert(!is_sb);
    assert(is_sb.error() == syscape::errc::not_supported);

    const auto tpm_res = syscape::security::tpm();
    assert(!tpm_res);
    assert(tpm_res.error() == syscape::errc::not_supported);

    const auto lsm_res = syscape::security::security_modules();
    assert(!lsm_res);
    assert(lsm_res.error() == syscape::errc::not_supported);

    const auto lock_res = syscape::security::lockdown();
    assert(!lock_res);
    assert(lock_res.error() == syscape::errc::not_supported);

    const auto sip_res = syscape::security::is_sip_enabled();
    assert(!sip_res);
    assert(sip_res.error() == syscape::errc::not_supported);

    const auto aslr_res = syscape::security::aslr();
    assert(!aslr_res);
    assert(aslr_res.error() == syscape::errc::not_supported);

    const auto vuln_res = syscape::security::cpu_vulnerabilities();
    assert(!vuln_res);
    assert(vuln_res.error() == syscape::errc::not_supported);

    const auto caps_res = syscape::security::capabilities();
    assert(!caps_res);
    assert(caps_res.error() == syscape::errc::not_supported);

    const auto privs_res = syscape::security::privileges();
    assert(!privs_res);
    assert(privs_res.error() == syscape::errc::not_supported);

    const auto enc_res = syscape::security::volume_encryption("/");
    assert(!enc_res);
    assert(enc_res.error() == syscape::errc::not_supported);

    const auto vols_res = syscape::security::encrypted_volumes();
    assert(!vols_res);
    assert(vols_res.error() == syscape::errc::not_supported);

    return 0;
}
