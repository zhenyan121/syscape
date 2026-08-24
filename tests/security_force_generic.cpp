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

    return 0;
}
