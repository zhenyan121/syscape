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

void test_synthetic_aslr_parser() {
    namespace scomm = syscape::detail::security_common;
    using syscape::security::aslr_mode;

    const auto d0 = scomm::parse_aslr_mode("0");
    expect(d0.has_value() && *d0 == aslr_mode::disabled, "0 must parse as disabled");

    const auto p1 = scomm::parse_aslr_mode("1");
    expect(p1.has_value() && *p1 == aslr_mode::partial, "1 must parse as partial");

    const auto f2 = scomm::parse_aslr_mode("2\n");
    expect(f2.has_value() && *f2 == aslr_mode::full, "2 with newline must parse as full");

    const auto u3 = scomm::parse_aslr_mode("3");
    expect(u3.has_value() && *u3 == aslr_mode::unknown, "3 must parse as unknown");

    const auto empty = scomm::parse_aslr_mode("");
    expect(!empty && empty.error() == syscape::errc::malformed_data, "Empty ASLR string must fail");

    const auto invalid = scomm::parse_aslr_mode("foo");
    expect(!invalid && invalid.error() == syscape::errc::malformed_data, "Non-numeric ASLR string must fail");
}

void test_synthetic_vulnerability_parser() {
    namespace scomm = syscape::detail::security_common;
    using syscape::security::mitigation_status;

    expect(scomm::parse_vulnerability_status("Not affected") == mitigation_status::not_affected,
           "Not affected must parse correctly");
    expect(scomm::parse_vulnerability_status("not affected\n") == mitigation_status::not_affected,
           "not affected with whitespace must parse correctly");
    expect(scomm::parse_vulnerability_status("Mitigation: PTI") == mitigation_status::mitigated,
           "Mitigation: PTI must parse as mitigated");
    expect(scomm::parse_vulnerability_status("Mitigated") == mitigation_status::mitigated,
           "Mitigated must parse as mitigated");
    expect(scomm::parse_vulnerability_status("Vulnerable: Clear CPU buffers") == mitigation_status::vulnerable,
           "Vulnerable must parse as vulnerable");
    expect(scomm::parse_vulnerability_status("Disabled") == mitigation_status::disabled,
           "Disabled must parse as disabled");
    expect(scomm::parse_vulnerability_status("Mitigation: Disabled") == mitigation_status::disabled,
           "Mitigation: Disabled must parse as disabled");
    expect(scomm::parse_vulnerability_status("KVM: not affected") == mitigation_status::not_affected,
           "KVM: not affected must parse correctly");
    expect(scomm::parse_vulnerability_status("KVM: Mitigation: Split lock detect") == mitigation_status::mitigated,
           "KVM: Mitigation must parse as mitigated");
    expect(scomm::parse_vulnerability_status("KVM: Vulnerable") == mitigation_status::vulnerable,
           "KVM: Vulnerable must parse as vulnerable");
    expect(scomm::parse_vulnerability_status("") == mitigation_status::unknown,
           "Empty string must parse as unknown");
    expect(scomm::parse_vulnerability_status("Unknown status") == mitigation_status::unknown,
           "Unknown string must parse as unknown");
}

void test_synthetic_hex_u64_parser() {
    namespace scomm = syscape::detail::security_common;

    const auto zero = scomm::parse_hex_u64("0000000000000000");
    expect(zero.has_value() && *zero == 0U, "Zero hex string must parse to 0");

    const auto with_prefix = scomm::parse_hex_u64("0x1f");
    expect(with_prefix.has_value() && *with_prefix == 0x1fU, "0x1f must parse to 31");

    const auto bnd = scomm::parse_hex_u64("000001ffffffffff");
    expect(bnd.has_value() && *bnd == 0x1FFFFFFFFFFULL, "CapBnd hex string must parse accurately");

    const auto wake = scomm::parse_hex_u64("0000000800000000");
    expect(wake.has_value() && *wake == (1ULL << 35U), "CapInh bit 35 must parse accurately");

    expect(!scomm::parse_hex_u64(""), "Empty string must fail");
    expect(!scomm::parse_hex_u64("0x"), "0x with no digits must fail");
    expect(!scomm::parse_hex_u64("0000000g"), "Non-hex char must fail");
}

void test_synthetic_capability_decoder() {
    namespace scomm = syscape::detail::security_common;

    const auto empty_caps = scomm::decode_linux_capabilities(0U);
    expect(empty_caps.empty(), "Zero mask must decode to empty vector");

    const auto chown_cap = scomm::decode_linux_capabilities(1ULL << 0U);
    expect(chown_cap.size() == 1U && chown_cap[0] == "cap_chown", "Bit 0 must decode to cap_chown");

    const auto admin_cap = scomm::decode_linux_capabilities(1ULL << 21U);
    expect(admin_cap.size() == 1U && admin_cap[0] == "cap_sys_admin", "Bit 21 must decode to cap_sys_admin");

    const auto checkpoint_cap = scomm::decode_linux_capabilities(1ULL << 40U);
    expect(checkpoint_cap.size() == 1U && checkpoint_cap[0] == "cap_checkpoint_restore", "Bit 40 must decode to cap_checkpoint_restore");

    const auto future_cap = scomm::decode_linux_capabilities(1ULL << 45U);
    expect(future_cap.size() == 1U && future_cap[0] == "cap_45", "Bit 45 must decode to cap_45");

    const auto multi_caps = scomm::decode_linux_capabilities((1ULL << 0U) | (1ULL << 21U));
    expect(multi_caps.size() == 2U && multi_caps[0] == "cap_chown" && multi_caps[1] == "cap_sys_admin",
           "Multi-bit mask must decode in bit order");
}

void test_synthetic_dm_uuid_parser() {
    namespace scomm = syscape::detail::security_common;
    using syscape::security::encryption_state;
    using syscape::security::encryption_type;

    const auto luks2 = scomm::parse_dm_uuid_encryption("CRYPT-LUKS2-xxx");
    expect(luks2.first == encryption_state::encrypted && luks2.second == encryption_type::luks,
           "CRYPT-LUKS2 must parse as encrypted LUKS");

    const auto luks1 = scomm::parse_dm_uuid_encryption("CRYPT-LUKS1-xxx");
    expect(luks1.first == encryption_state::encrypted && luks1.second == encryption_type::luks,
           "CRYPT-LUKS1 must parse as encrypted LUKS");

    const auto plain = scomm::parse_dm_uuid_encryption("CRYPT-PLAIN-xxx");
    expect(plain.first == encryption_state::encrypted && plain.second == encryption_type::dm_crypt,
           "CRYPT-PLAIN must parse as encrypted dm_crypt");

    const auto other = scomm::parse_dm_uuid_encryption("CRYPT-CUSTOM-xxx");
    expect(other.first == encryption_state::encrypted && other.second == encryption_type::other,
           "CRYPT-CUSTOM must parse as encrypted other");

    const auto unenc = scomm::parse_dm_uuid_encryption("LVM-xxx");
    expect(unenc.first == encryption_state::unknown && unenc.second == encryption_type::unknown,
           "LVM UUID must parse as unknown encryption");

    const auto empty = scomm::parse_dm_uuid_encryption("");
    expect(empty.first == encryption_state::unknown && empty.second == encryption_type::unknown,
           "Empty UUID must parse as unknown encryption");
}

void test_synthetic_capabilities_backend() {
    namespace sbackend = syscape::detail::security_backend;

    const auto fixture = make_fixture_path("status");
    std::filesystem::create_directories(fixture);

    const auto status_file = fixture / "status";
    {
        std::ofstream out(status_file);
        out << "Name:\tmyprocess\n"
            << "State:\tR (running)\n"
            << "CapInh:\t0000000000000000\n"
            << "CapPrm:\t0000000000200000\n"
            << "CapEff:\t0000000000200000\n"
            << "CapBnd:\t000001ffffffffff\n"
            << "CapAmb:\t0000000000000000\n";
    }

    const auto caps_res = sbackend::capabilities_at(status_file.c_str());
    expect(caps_res.has_value(), "capabilities_at must succeed on valid fixture");
    if (caps_res) {
        expect(caps_res->effective.size() == 1U && caps_res->effective[0] == "cap_sys_admin",
               "Effective cap must be cap_sys_admin (bit 21)");
        expect(caps_res->permitted.size() == 1U && caps_res->permitted[0] == "cap_sys_admin",
               "Permitted cap must be cap_sys_admin (bit 21)");
        expect(caps_res->bounding.size() == 41U,
               "Bounding set must contain 41 capabilities");
        expect(caps_res->inheritable.empty(),
               "Inheritable set must be empty");
        expect(caps_res->ambient.empty(),
               "Ambient set must be empty");
    }

    // Missing status file
    const auto missing = sbackend::capabilities_at((fixture / "nonexistent").c_str());
    expect(!missing && missing.error() == syscape::errc::not_supported,
           "Missing status file must return not_supported");

    // Malformed status file (no Cap lines)
    const auto bad_status = fixture / "bad_status";
    {
        std::ofstream out(bad_status);
        out << "Name:\tbad\nPID:\t123\n";
    }
    const auto bad_res = sbackend::capabilities_at(bad_status.c_str());
    expect(!bad_res && bad_res.error() == syscape::errc::malformed_data,
           "Status file without Cap lines must fail with malformed_data");

    std::filesystem::remove_all(fixture);
}

void test_synthetic_cpu_vulnerabilities_backend() {
    namespace sbackend = syscape::detail::security_backend;
    using syscape::security::mitigation_status;

    const auto fixture = make_fixture_path("vuln");
    std::filesystem::create_directories(fixture);

    {
        std::ofstream out(fixture / "meltdown");
        out << "Not affected\n";
    }
    {
        std::ofstream out(fixture / "spectre_v1");
        out << "Mitigation: usercopy/swapgs barriers\n";
    }
    {
        std::ofstream out(fixture / "retbleed");
        out << "Vulnerable\n";
    }

    const auto list_res = sbackend::cpu_vulnerabilities_at(fixture.c_str());
    expect(list_res.has_value(), "cpu_vulnerabilities_at must succeed on valid fixture");
    if (list_res) {
        expect(list_res->size() == 3U, "Must parse 3 vulnerability entries");
        // Sorted alphabetically: meltdown, retbleed, spectre_v1
        expect((*list_res)[0].name == "meltdown" && (*list_res)[0].status == mitigation_status::not_affected,
               "meltdown must be not_affected");
        expect((*list_res)[1].name == "retbleed" && (*list_res)[1].status == mitigation_status::vulnerable,
               "retbleed must be vulnerable");
        expect((*list_res)[2].name == "spectre_v1" && (*list_res)[2].status == mitigation_status::mitigated,
               "spectre_v1 must be mitigated");
    }

    // Missing directory
    const auto missing = sbackend::cpu_vulnerabilities_at((fixture / "nonexistent").c_str());
    expect(!missing && missing.error() == syscape::errc::not_supported,
           "Missing vulnerabilities dir must return not_supported");

    std::filesystem::remove_all(fixture);
}

void test_synthetic_aslr_backend() {
    namespace sbackend = syscape::detail::security_backend;
    using syscape::security::aslr_mode;

    const auto fixture = make_fixture_path("aslr");
    std::filesystem::create_directories(fixture);

    const auto aslr_file = fixture / "randomize_va_space";
    {
        std::ofstream out(aslr_file);
        out << "2\n";
    }

    const auto res = sbackend::aslr_at(aslr_file.c_str());
    expect(res.has_value() && *res == aslr_mode::full, "aslr_at must return full on 2");

    const auto missing = sbackend::aslr_at((fixture / "nonexistent").c_str());
    expect(!missing && missing.error() == syscape::errc::not_supported,
           "Missing ASLR file must return not_supported");

    std::filesystem::remove_all(fixture);
}

void test_synthetic_volume_encryption_backend() {
    namespace sbackend = syscape::detail::security_backend;
    using syscape::security::encryption_state;
    using syscape::security::encryption_type;

    const auto fixture = make_fixture_path("volenc");
    const auto block_dir = fixture / "block";
    const auto mounts_file = fixture / "mounts";
    const auto crypt_target = fixture / "mnt" / "secret";
    std::filesystem::create_directories(block_dir / "dm-0" / "dm");
    std::filesystem::create_directories(crypt_target);

    // Set up dm-0 with LUKS2 uuid
    {
        std::ofstream out(block_dir / "dm-0" / "dm" / "uuid");
        out << "CRYPT-LUKS2-12345678-abcd\n";
    }
    {
        std::ofstream out(block_dir / "dm-0" / "dm" / "name");
        out << "cryptroot\n";
    }

    // Set up mounts
    {
        std::ofstream out(mounts_file);
        out << "/dev/mapper/cryptroot " << crypt_target.string() << " ext4 rw,relatime 0 0\n"
            << "/dev/sda1 / ext4 rw,relatime 0 0\n";
    }

    const auto enc_res = sbackend::volume_encryption_at(
        crypt_target.string(), block_dir.c_str(), mounts_file.c_str());
    expect(enc_res.has_value(), "volume_encryption_at must succeed on valid encrypted path");
    if (enc_res) {
        expect(enc_res->state == encryption_state::encrypted,
               "Target must be classified as encrypted");
        expect(enc_res->type == encryption_type::luks,
               "Target must be classified as LUKS");
    }

    // Set up LVM on top of LUKS: dm-1 is LVM, slave is dm-0 (LUKS)
    const auto lvm_target = fixture / "mnt" / "lvm_data\040dir";
    std::filesystem::create_directories(block_dir / "dm-1" / "dm");
    std::filesystem::create_directories(block_dir / "dm-1" / "slaves");
    std::filesystem::create_directories(lvm_target);
    {
        std::ofstream out(block_dir / "dm-1" / "dm" / "uuid");
        out << "LVM-abcdef123456\n";
    }
    {
        std::ofstream out(block_dir / "dm-1" / "dm" / "name");
        out << "vg-lv_data\n";
    }
    // Create slave link/dir dm-0 inside dm-1/slaves
    std::filesystem::create_directories(block_dir / "dm-1" / "slaves" / "dm-0");
    // Update mounts with octal escaped space \040
    {
        std::ofstream out(mounts_file);
        out << "/dev/mapper/cryptroot " << crypt_target.string() << " ext4 rw,relatime 0 0\n"
            << "/dev/mapper/vg-lv_data " << fixture.string() << "/mnt/lvm_data\\040dir ext4 rw,relatime 0 0\n"
            << "/dev/sda1 / ext4 rw,relatime 0 0\n";
    }

    // Test LVM on LUKS with space in mount path
    const auto lvm_res = sbackend::volume_encryption_at(
        lvm_target.string(), block_dir.c_str(), mounts_file.c_str());
    expect(lvm_res.has_value(), "volume_encryption_at must succeed on LVM-on-LUKS path");
    if (lvm_res) {
        expect(lvm_res->state == encryption_state::encrypted,
               "LVM over LUKS must be classified as encrypted");
        expect(lvm_res->type == encryption_type::luks,
               "LVM over LUKS must be classified as LUKS");
    }

    // Test multi-slave: dm-2 has dm-0 (LUKS) and sdb1 (unencrypted raw) -> mixed
    const auto mixed_target = fixture / "mnt" / "mixed_vol";
    std::filesystem::create_directories(block_dir / "dm-2" / "dm");
    std::filesystem::create_directories(block_dir / "dm-2" / "slaves" / "dm-0");
    std::filesystem::create_directories(block_dir / "dm-2" / "slaves" / "sdb1");
    std::filesystem::create_directories(block_dir / "sdb1");
    std::filesystem::create_directories(mixed_target);
    {
        std::ofstream out(block_dir / "dm-2" / "dm" / "uuid");
        out << "LVM-mixed123456\n";
    }
    {
        std::ofstream out(block_dir / "dm-2" / "dm" / "name");
        out << "vg-lv_mixed\n";
    }
    {
        std::ofstream out(mounts_file, std::ios::app);
        out << "/dev/mapper/vg-lv_mixed " << mixed_target.string() << " ext4 rw,relatime 0 0\n";
    }

    const auto mixed_res = sbackend::volume_encryption_at(
        mixed_target.string(), block_dir.c_str(), mounts_file.c_str());
    expect(mixed_res.has_value(), "volume_encryption_at must succeed on multi-slave layout");
    if (mixed_res) {
        expect(mixed_res->state == encryption_state::unknown,
               "Multi-slave with encrypted and unknown members must report unknown state");
    }

    // Check encrypted_volumes_at
    const auto list_res = sbackend::encrypted_volumes_at(
        block_dir.c_str(), mounts_file.c_str());
    expect(list_res.has_value(), "encrypted_volumes_at must succeed");
    if (list_res) {
        expect(!list_res->empty(), "Should enumerate encrypted volumes");
    }

    // Missing mounts must return not_supported
    const auto missing_mounts = sbackend::volume_encryption_at(
        crypt_target.string(), block_dir.c_str(), (fixture / "no_mounts").c_str());
    expect(!missing_mounts && missing_mounts.error() == syscape::errc::not_supported,
           "Missing mounts table must return not_supported");

    // Missing block dir in encrypted_volumes must return not_supported
    const auto missing_block = sbackend::encrypted_volumes_at(
        (fixture / "no_block").c_str(), mounts_file.c_str());
    expect(!missing_block && missing_block.error() == syscape::errc::not_supported,
           "Missing block dir must return not_supported");

    // Path validation
    expect(!syscape::security::volume_encryption(""),
           "Empty path must fail validation");
    expect(!syscape::security::volume_encryption(std::string_view("abc\0def", 7)),
           "Path with null char must fail validation");
    const char invalid_utf8[] = {'/', static_cast<char>(0xFF), static_cast<char>(0xFE), '\0'};
    expect(!syscape::security::volume_encryption(invalid_utf8),
           "Invalid UTF-8 path must fail validation");
    expect(!syscape::security::volume_encryption("/nonexistent_syscape_test_path_12345"),
           "Nonexistent path must fail with not_found");

    std::filesystem::remove_all(fixture);
}

void test_linux_live_queries() {
    using syscape::security::secure_boot_state;
    using syscape::security::lockdown_mode;
    using syscape::security::aslr_mode;

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

    // ASLR live query
    const auto aslr_res = syscape::security::aslr();
    if (aslr_res) {
        expect(*aslr_res == aslr_mode::disabled ||
               *aslr_res == aslr_mode::partial ||
               *aslr_res == aslr_mode::full ||
               *aslr_res == aslr_mode::unknown,
               "aslr() must return a valid aslr_mode");
    } else {
        expect(static_cast<bool>(aslr_res.error()),
               "A failed aslr query must carry an error");
    }

    // CPU vulnerabilities live query
    const auto vuln_res = syscape::security::cpu_vulnerabilities();
    if (vuln_res) {
        expect(!vuln_res->empty(), "Linux sysfs cpu vulnerabilities should expose entries");
        for (const auto& entry : *vuln_res) {
            expect(!entry.name.empty(), "Vulnerability name must not be empty");
            expect(!entry.raw_description.empty(), "Vulnerability description must not be empty");
        }
    } else {
        expect(static_cast<bool>(vuln_res.error()),
               "A failed cpu_vulnerabilities query must carry an error");
    }

    // Process capabilities live query
    const auto caps_res = syscape::security::capabilities();
    if (caps_res) {
        // Capabilities can legitimately be empty in restricted or containerized environments.
        for (const auto& cap : caps_res->effective) {
            expect(!cap.empty(), "Capability name must not be empty");
        }
        for (const auto& cap : caps_res->permitted) {
            expect(!cap.empty(), "Capability name must not be empty");
        }
    } else {
        expect(static_cast<bool>(caps_res.error()),
               "A failed capabilities query must carry an error");
    }

    // Process privileges live query
    const auto privs_res = syscape::security::privileges();
    if (privs_res) {
        for (const auto& priv : *privs_res) {
            expect(!priv.name.empty(), "Privilege name must not be empty");
        }
    } else {
        expect(static_cast<bool>(privs_res.error()),
               "A failed privileges query must carry an error");
    }

    // Volume encryption live query
    const auto enc_res = syscape::security::volume_encryption("/");
    if (enc_res) {
        expect(enc_res->state == syscape::security::encryption_state::unencrypted ||
               enc_res->state == syscape::security::encryption_state::encrypted ||
               enc_res->state == syscape::security::encryption_state::unknown,
               "volume_encryption(/) state must be valid enum");
    }

    // Encrypted volumes live query
    const auto enc_vols = syscape::security::encrypted_volumes();
    if (!enc_vols) {
        expect(enc_vols.error() == syscape::errc::not_supported ||
               static_cast<bool>(enc_vols.error()),
               "Failed encrypted_volumes query must carry an error");
    }
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
    test_synthetic_aslr_parser();
    test_synthetic_vulnerability_parser();
    test_synthetic_hex_u64_parser();
    test_synthetic_capability_decoder();
    test_synthetic_dm_uuid_parser();
    test_synthetic_capabilities_backend();
    test_synthetic_cpu_vulnerabilities_backend();
    test_synthetic_aslr_backend();
    test_synthetic_volume_encryption_backend();
    test_linux_live_queries();

    return failures == 0 ? 0 : 1;
}
