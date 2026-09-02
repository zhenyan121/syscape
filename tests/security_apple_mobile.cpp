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
    const auto aslr = syscape::security::aslr();
    expect(aslr.has_value() || aslr.error() == syscape::errc::not_supported,
           "aslr query must succeed or report not_supported");

    const auto tpm = syscape::security::tpm();
    expect(tpm.has_value(), "tpm query must succeed");

    const auto secboot = syscape::security::secure_boot();
    expect(secboot.has_value() ||
               secboot.error() == syscape::errc::not_supported,
           "secure boot query must succeed or report not_supported");

    const auto empty_vol = syscape::security::volume_encryption("");
    expect(!empty_vol && empty_vol.error() == syscape::errc::invalid_argument,
           "empty path volume encryption query must report invalid_argument");

    const std::string_view nul_path("/abc\0def", 8);
    const auto nul_vol = syscape::security::volume_encryption(nul_path);
    expect(!nul_vol && nul_vol.error() == syscape::errc::invalid_argument,
           "null-embedded path volume encryption query must report "
           "invalid_argument");

    const std::string_view bad_utf8("\xFF\xFE", 2);
    const auto bad_vol = syscape::security::volume_encryption(bad_utf8);
    expect(!bad_vol && bad_vol.error() == syscape::errc::invalid_encoding,
           "invalid utf-8 path volume encryption query must report "
           "invalid_encoding");

    const auto vol = syscape::security::volume_encryption("/");
    expect(vol.has_value() || vol.error() == syscape::errc::not_supported,
           "volume encryption query must succeed or report not_supported");

    const auto enc_vols = syscape::security::encrypted_volumes();
    expect(enc_vols.has_value() ||
               enc_vols.error() == syscape::errc::not_supported,
           "encrypted volumes query must succeed or report not_supported");
}

} // namespace

int main() {
    test_security_queries();
    return failures == 0 ? 0 : 1;
}
