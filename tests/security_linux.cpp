#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

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

const char* global_variable_guid =
    "8be4df61-93ca-11d2-aa0d-00e098032b8c";

std::filesystem::path make_fixture_path(const char* name) {
    return std::filesystem::temp_directory_path() /
           (std::string("syscape-security-") + name + "-" +
            std::to_string(static_cast<long long>(::getpid())));
}

std::filesystem::path efivar_path(
    const std::filesystem::path& root, const char* name) {
    return root / "efivars" /
           (std::string(name) + "-" + global_variable_guid);
}

void write_efivar(
    const std::filesystem::path& root, const char* name,
    unsigned char value) {
    const auto path = efivar_path(root, name);
    std::filesystem::create_directories(path.parent_path());
    const unsigned char bytes[] = {0x06U, 0x00U, 0x00U, 0x00U, value};
    std::ofstream output(path, std::ios::binary);
    output.write(
        reinterpret_cast<const char*>(bytes),
        static_cast<std::streamsize>(sizeof(bytes)));
}

void test_synthetic_efivar_secure_boot() {
    namespace scomm = syscape::detail::security_common;
    using syscape::security::secure_boot_state;

    // 5-byte efivar payload: 4-byte attribute + 1-byte value 1 (enabled)
    const unsigned char enabled_5b[] = { 0x06, 0x00, 0x00, 0x00, 0x01 };
    const auto res_enabled = scomm::parse_efivar_secure_boot_payload(enabled_5b, sizeof(enabled_5b));
    expect(res_enabled.has_value() && *res_enabled == secure_boot_state::enabled,
           "5-byte efivar with value 1 must parse as enabled");

    // 5-byte efivar payload: 4-byte attribute + 1-byte value 0 (disabled)
    const unsigned char disabled_5b[] = { 0x06, 0x00, 0x00, 0x00, 0x00 };
    const auto res_disabled = scomm::parse_efivar_secure_boot_payload(disabled_5b, sizeof(disabled_5b));
    expect(res_disabled.has_value() && *res_disabled == secure_boot_state::disabled,
           "5-byte efivar with value 0 must parse as disabled");

    // 1-byte legacy payload: value 1
    const unsigned char enabled_1b[] = { 0x01 };
    const auto res_legacy_en = scomm::parse_efivar_secure_boot_payload(enabled_1b, sizeof(enabled_1b));
    expect(res_legacy_en.has_value() && *res_legacy_en == secure_boot_state::enabled,
           "1-byte legacy payload with value 1 must parse as enabled");

    // 1-byte legacy payload: value 0
    const unsigned char disabled_1b[] = { 0x00 };
    const auto res_legacy_dis = scomm::parse_efivar_secure_boot_payload(disabled_1b, sizeof(disabled_1b));
    expect(res_legacy_dis.has_value() && *res_legacy_dis == secure_boot_state::disabled,
           "1-byte legacy payload with value 0 must parse as disabled");

    // Invalid sizes
    expect(!scomm::parse_efivar_secure_boot_payload(nullptr, 5U),
           "Null buffer must report error");
    expect(!scomm::parse_efivar_secure_boot_payload(enabled_5b, 0U),
           "Zero size must report error");
    expect(!scomm::parse_efivar_secure_boot_payload(enabled_5b, 2U),
           "Invalid size 2 must report error");
    expect(!scomm::parse_efivar_secure_boot_payload(enabled_5b, 4U),
           "Invalid size 4 must report error");
    const unsigned char oversized[] = {
        0x06U, 0x00U, 0x00U, 0x00U, 0x01U, 0x00U
    };
    expect(!scomm::parse_efivar_secure_boot_payload(oversized, sizeof(oversized)),
           "Efivar payload with trailing bytes must report error");

    // Invalid value byte (e.g. 0x02)
    const unsigned char invalid_val[] = { 0x06, 0x00, 0x00, 0x00, 0x02 };
    expect(!scomm::parse_efivar_secure_boot_payload(invalid_val, sizeof(invalid_val)),
           "Invalid value byte must report error");
}

void test_synthetic_secure_boot_backend() {
    using syscape::security::secure_boot_state;
    namespace backend = syscape::detail::security_backend;

    const auto fixture = make_fixture_path("efi");
    std::filesystem::remove_all(fixture);

    const auto absent = backend::secure_boot_at(fixture.string());
    expect(!absent && absent.error() == syscape::errc::not_supported,
           "Missing EFI root must report not_supported");

    std::filesystem::create_directories(fixture / "efivars");
    const auto missing_variable = backend::secure_boot_at(fixture.string());
    expect(!missing_variable &&
               missing_variable.error() == syscape::errc::not_supported,
           "Missing SecureBoot variable must report not_supported");

    write_efivar(fixture, "SecureBoot", 1U);
    const auto enabled = backend::secure_boot_at(fixture.string());
    expect(enabled && *enabled == secure_boot_state::enabled,
           "SecureBoot value 1 must report enabled");

    write_efivar(fixture, "SecureBoot", 0U);
    write_efivar(fixture, "AuditMode", 1U);
    const auto audit = backend::secure_boot_at(fixture.string());
    expect(audit && *audit == secure_boot_state::audit,
           "AuditMode value 1 must report audit");

    write_efivar(fixture, "AuditMode", 0U);
    write_efivar(fixture, "SetupMode", 1U);
    const auto setup = backend::secure_boot_at(fixture.string());
    expect(setup && *setup == secure_boot_state::audit,
           "SetupMode value 1 must report the non-enforcing audit/setup state");

    write_efivar(fixture, "SetupMode", 0U);
    const auto disabled = backend::secure_boot_at(fixture.string());
    expect(disabled && *disabled == secure_boot_state::disabled,
           "Zero SecureBoot, AuditMode, and SetupMode must report disabled");

    std::filesystem::remove_all(fixture / "efivars");
    const auto legacy_data = fixture / "vars" /
        (std::string("SecureBoot-") + global_variable_guid) / "data";
    std::filesystem::create_directories(legacy_data.parent_path());
    {
        std::ofstream output(legacy_data, std::ios::binary);
        output.put(static_cast<char>(1));
    }
    const auto legacy_enabled = backend::secure_boot_at(fixture.string());
    expect(legacy_enabled && *legacy_enabled == secure_boot_state::enabled,
           "The legacy one-byte SecureBoot interface must remain supported");

    std::filesystem::remove_all(fixture / "vars");
    std::filesystem::create_directories(fixture / "efivars");
    std::filesystem::remove(efivar_path(fixture, "SecureBoot"));
    std::filesystem::create_directory(efivar_path(fixture, "SecureBoot"));
    const auto io_failure = backend::secure_boot_at(fixture.string());
    expect(!io_failure &&
               io_failure.error() == std::errc::is_a_directory,
           "SecureBoot read failures must not be reported as disabled");

    std::filesystem::remove_all(fixture);
}

void test_synthetic_securityfs_backends() {
    namespace backend = syscape::detail::security_backend;
    using syscape::security::lockdown_mode;

    const auto fixture = make_fixture_path("securityfs");
    std::filesystem::remove_all(fixture);
    std::filesystem::create_directories(fixture);
    const auto lsm_path = fixture / "lsm";
    const auto lockdown_path = fixture / "lockdown";

    {
        std::ofstream output(lsm_path);
        output << "capability,landlock,lockdown,yama,bpf";
    }
    const auto modules = backend::security_modules_at(lsm_path.c_str());
    expect(modules && modules->size() == 5U && (*modules)[0] == "capability",
           "The authoritative LSM list must be returned unchanged");

    {
        std::ofstream output(lsm_path, std::ios::trunc);
    }
    const auto empty_modules = backend::security_modules_at(lsm_path.c_str());
    expect(!empty_modules && empty_modules.error() == syscape::errc::malformed_data,
           "An empty authoritative LSM list must be malformed");

    std::filesystem::remove(lsm_path);
    const auto missing_modules = backend::security_modules_at(lsm_path.c_str());
    expect(!missing_modules &&
               missing_modules.error() == syscape::errc::not_supported,
           "A missing authoritative LSM interface must report not_supported");

    {
        std::ofstream output(lsm_path, std::ios::binary);
        output << "capability,";
        output.put(static_cast<char>(0xFF));
    }
    const auto invalid_modules = backend::security_modules_at(lsm_path.c_str());
    expect(!invalid_modules &&
               invalid_modules.error() == syscape::errc::invalid_encoding,
           "Non-UTF-8 LSM names must report invalid_encoding");

    {
        std::ofstream output(lockdown_path);
        output << "none [integrity] confidentiality";
    }
    const auto lockdown = backend::lockdown_at(lockdown_path.c_str());
    expect(lockdown && *lockdown == lockdown_mode::integrity,
           "The active lockdown mode must be parsed");

    {
        std::ofstream output(lockdown_path, std::ios::trunc);
        output << "malformed";
    }
    const auto malformed_lockdown = backend::lockdown_at(lockdown_path.c_str());
    expect(!malformed_lockdown &&
               malformed_lockdown.error() == syscape::errc::malformed_data,
           "Malformed lockdown data must report malformed_data");

    {
        std::ofstream output(lockdown_path, std::ios::trunc);
        output << "none integrity [future_mode]";
    }
    const auto future_lockdown = backend::lockdown_at(lockdown_path.c_str());
    expect(future_lockdown && *future_lockdown == lockdown_mode::unknown,
           "A well-formed future lockdown mode must report unknown");

    std::filesystem::remove_all(fixture);
}

void test_synthetic_tpm_backend() {
    namespace backend = syscape::detail::security_backend;
    using syscape::security::tpm_version;

    const auto fixture = make_fixture_path("tpm");
    const auto device = fixture / "tpm0";
    std::filesystem::remove_all(fixture);
    std::filesystem::create_directories(device / "device");
    {
        std::ofstream version(device / "tpm_version_major");
        version << "2\n";
        std::ofstream class_uevent(device / "uevent");
        class_uevent << "MAJOR=10\nMINOR=224\nDEVNAME=tpm0\n";
        std::ofstream device_uevent(device / "device" / "uevent");
        device_uevent << "DRIVER=tpm_crb_acpi\nMODALIAS=acpi:MSFT0101:\n";
    }

    const auto tpm = backend::tpm_at(fixture.c_str());
    expect(tpm && tpm->present && tpm->version == tpm_version::v2_0,
           "A fixture TPM must be detected as TPM 2.0");
    expect(tpm && tpm->description.has_value() &&
               *tpm->description == "tpm_crb_acpi",
           "The TPM driver must come from device/uevent");

    std::filesystem::remove(device / "device" / "uevent");
    {
        std::ofstream class_uevent(device / "uevent", std::ios::trunc);
        class_uevent << "DRIVER=fallback_driver\n";
    }
    const auto fallback = backend::tpm_at(fixture.c_str());
    expect(fallback && fallback->description.has_value() &&
               *fallback->description == "fallback_driver",
           "A class uevent DRIVER field must remain a fallback");

    {
        std::ofstream device_uevent(
            device / "device" / "uevent", std::ios::binary);
        device_uevent << "DRIVER=";
        device_uevent.put(static_cast<char>(0xFF));
        device_uevent << '\n';
    }
    const auto invalid_description = backend::tpm_at(fixture.c_str());
    expect(!invalid_description &&
               invalid_description.error() == syscape::errc::invalid_encoding,
           "A non-UTF-8 TPM description must report invalid_encoding");

    std::filesystem::remove_all(fixture);
}

void test_synthetic_lockdown_parser() {
    namespace scomm = syscape::detail::security_common;
    using syscape::security::lockdown_mode;

    const auto none =
        scomm::parse_lockdown_line("[none] integrity confidentiality");
    expect(none && *none == lockdown_mode::none,
           "Bracketed [none] must parse as none");

    const auto integrity =
        scomm::parse_lockdown_line("none [integrity] confidentiality");
    expect(integrity && *integrity == lockdown_mode::integrity,
           "Bracketed [integrity] must parse as integrity");

    const auto confidentiality =
        scomm::parse_lockdown_line("none integrity [confidentiality]");
    expect(confidentiality && *confidentiality == lockdown_mode::confidentiality,
           "Bracketed [confidentiality] must parse as confidentiality");

    const auto whitespace =
        scomm::parse_lockdown_line("  [  integrity  ]  ");
    expect(whitespace && *whitespace == lockdown_mode::integrity,
           "Whitespace around bracketed integrity must be trimmed");

    const auto no_brackets =
        scomm::parse_lockdown_line("none integrity confidentiality");
    expect(!no_brackets &&
               no_brackets.error() == syscape::errc::malformed_data,
           "Line without brackets must report malformed_data");

    const auto future = scomm::parse_lockdown_line("[unsupported_mode]");
    expect(future && *future == lockdown_mode::unknown,
           "Unrecognized bracketed mode must parse as unknown");

    const auto empty = scomm::parse_lockdown_line("");
    expect(!empty && empty.error() == syscape::errc::malformed_data,
           "Empty string must report malformed_data");
}

void test_synthetic_lsm_parser() {
    namespace scomm = syscape::detail::security_common;

    const auto modules =
        scomm::parse_lsm_string("capability,landlock,lockdown,yama,bpf");
    expect(modules.size() == 5U, "Must parse 5 LSM modules");
    expect(modules[0] == "capability", "Module 0 must be capability");
    expect(modules[1] == "landlock", "Module 1 must be landlock");
    expect(modules[2] == "lockdown", "Module 2 must be lockdown");
    expect(modules[3] == "yama", "Module 3 must be yama");
    expect(modules[4] == "bpf", "Module 4 must be bpf");

    // Whitespace and deduplication
    const auto deduplicated =
        scomm::parse_lsm_string("selinux, apparmor , selinux , smack");
    expect(deduplicated.size() == 3U, "Duplicates must be removed");
    expect(deduplicated[0] == "selinux", "First must be selinux");
    expect(deduplicated[1] == "apparmor", "Second must be apparmor");
    expect(deduplicated[2] == "smack", "Third must be smack");

    // Empty string
    expect(scomm::parse_lsm_string("").empty(), "Empty string yields empty vector");
}

void test_synthetic_tpm_version_parser() {
    namespace scomm = syscape::detail::security_common;
    using syscape::security::tpm_version;

    const auto v2 = scomm::parse_tpm_version_string("2");
    expect(v2.first == tpm_version::v2_0 && v2.second == "2.0",
           "Major version 2 must parse as v2_0 with 2.0 string");

    const auto v2_full = scomm::parse_tpm_version_string(" 2.0 \n");
    expect(v2_full.first == tpm_version::v2_0 && v2_full.second == "2.0",
           "Trimmed version 2.0 must parse as v2_0");

    const auto v1 = scomm::parse_tpm_version_string("1");
    expect(v1.first == tpm_version::v1_2 && v1.second == "1.2",
           "Major version 1 must parse as v1_2 with 1.2 string");

    const auto other = scomm::parse_tpm_version_string("3.0");
    expect(other.first == tpm_version::other && other.second == "3.0",
           "Other version 3.0 must parse as other");

    const auto empty = scomm::parse_tpm_version_string("");
    expect(empty.first == tpm_version::unknown && empty.second.empty(),
           "Empty version string must parse as unknown");
}

void test_linux_live_queries() {
    using syscape::security::secure_boot_state;
    using syscape::security::lockdown_mode;

    // Secure Boot live query
    const auto sb_res = syscape::security::secure_boot();
    if (sb_res) {
        expect(*sb_res == secure_boot_state::enabled ||
               *sb_res == secure_boot_state::disabled ||
               *sb_res == secure_boot_state::audit,
               "Live secure_boot() must return a valid state enum");
    } else {
        expect(static_cast<bool>(sb_res.error()),
               "A failed Secure Boot query must carry an error");
    }

    const auto is_sb = syscape::security::is_secure_boot_enabled();
    if (is_sb) {
        if (sb_res && *sb_res == secure_boot_state::enabled) {
            expect(*is_sb == true, "is_secure_boot_enabled must agree with secure_boot()");
        } else {
            expect(*is_sb == false, "is_secure_boot_enabled must be false when not enabled");
        }
    }

    // TPM live query
    const auto tpm_res = syscape::security::tpm();
    if (tpm_res) {
        if (tpm_res->present) {
            expect(tpm_res->device_id.has_value(),
                   "Present TPM must have device_id");
        }
    } else {
        expect(static_cast<bool>(tpm_res.error()),
               "A failed TPM query must carry an error");
    }

    // Security modules live query
    const auto lsm_res = syscape::security::security_modules();
    if (lsm_res) {
        expect(!lsm_res->empty(), "Linux kernel should report at least one security module");
    } else {
        expect(static_cast<bool>(lsm_res.error()),
               "A failed LSM query must carry an error");
    }

    // Kernel lockdown live query
    const auto lock_res = syscape::security::lockdown();
    if (lock_res) {
        expect(*lock_res == lockdown_mode::none ||
               *lock_res == lockdown_mode::integrity ||
               *lock_res == lockdown_mode::confidentiality ||
               *lock_res == lockdown_mode::unknown,
               "lockdown() must return a valid lockdown_mode");
    } else {
        expect(static_cast<bool>(lock_res.error()),
               "A failed lockdown query must carry an error");
    }

    // SIP on Linux
    const auto sip_res = syscape::security::is_sip_enabled();
    expect(!sip_res && sip_res.error() == syscape::errc::not_supported,
           "SIP query on Linux must report not_supported");
}

} // namespace

int main() {
    test_synthetic_efivar_secure_boot();
    test_synthetic_secure_boot_backend();
    test_synthetic_securityfs_backends();
    test_synthetic_tpm_backend();
    test_synthetic_lockdown_parser();
    test_synthetic_lsm_parser();
    test_synthetic_tpm_version_parser();
    test_linux_live_queries();

    return failures == 0 ? 0 : 1;
}
