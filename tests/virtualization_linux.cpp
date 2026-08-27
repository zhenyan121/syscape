#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include <syscape/virtualization.hpp>
#include <syscape/detail/virtualization/common.hpp>
#include <syscape/detail/virtualization/linux.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_cpuid_signature_decoding() {
    namespace common = syscape::detail::virtualization_common;

    // "KVMKVMKVM\0\0\0"
    // ebx = 'K' | ('V'<<8) | ('M'<<16) | ('K'<<24) = 0x4B4D564B
    // ecx = 'V' | ('M'<<8) | ('K'<<16) | ('V'<<24) = 0x564B4D56
    // edx = 'M' | (0<<8)   | (0<<16)   | (0<<24)   = 0x0000004D
    const std::string kvm = common::decode_cpuid_signature(
        0x4B4D564BU, 0x564B4D56U, 0x0000004DU);
    expect(kvm == "KVMKVMKVM", "KVM signature must decode and trim trailing nulls");
    expect(common::classify_cpuid_signature(kvm) ==
               common::hypervisor_type::kvm,
           "KVM signature must classify as KVM");

    // VMware: "VMwareVMware"
    // ebx: "VMwa" = 0x61774D56
    // ecx: "reVM" = 0x4D566572
    // edx: "ware" = 0x65726177
    const std::string vmware = common::decode_cpuid_signature(
        0x61774D56U, 0x4D566572U, 0x65726177U);
    expect(vmware == "VMwareVMware", "VMware signature must decode exactly");
    expect(common::classify_cpuid_signature(vmware) ==
               common::hypervisor_type::vmware,
           "VMware signature must classify as VMware");

    // Hyper-V: "Microsoft Hv"
    expect(common::classify_cpuid_signature("Microsoft Hv") ==
               common::hypervisor_type::hyper_v,
           "Microsoft Hv must classify as Hyper-V");

    // VirtualBox: "VBoxVBoxVBox"
    expect(common::classify_cpuid_signature("VBoxVBoxVBox") ==
               common::hypervisor_type::virtualbox,
           "VBoxVBoxVBox must classify as VirtualBox");

    // QEMU TCG: "TCGTCGTCGTCG"
    expect(common::classify_cpuid_signature("TCGTCGTCGTCG") ==
               common::hypervisor_type::qemu,
           "TCGTCGTCGTCG must classify as QEMU");

    // Xen: "XenVMMXenVMM"
    expect(common::classify_cpuid_signature("XenVMMXenVMM") ==
               common::hypervisor_type::xen,
           "XenVMMXenVMM must classify as Xen");

    // bhyve: "bhyve bhyve "
    expect(common::classify_cpuid_signature("bhyve bhyve ") ==
               common::hypervisor_type::bhyve,
           "bhyve signature must classify as bhyve");

    // Parallels: "prl hyperv  "
    expect(common::classify_cpuid_signature("prl hyperv  ") ==
               common::hypervisor_type::parallels,
           "prl hyperv must classify as Parallels");

    // ACRN: "ACRNACRNACRN"
    expect(common::classify_cpuid_signature("ACRNACRNACRN") ==
               common::hypervisor_type::acrn,
           "ACRN signature must classify as ACRN");

    // QNX: "QNXQVMBSYS\0"
    expect(common::classify_cpuid_signature("QNXQVMBSYS\0") ==
               common::hypervisor_type::qnx_hypervisor,
           "QNX signature must classify as QNX");

    // Other unrecognized signature
    expect(common::classify_cpuid_signature("CustomHV123") ==
               common::hypervisor_type::other,
           "Unrecognized non-empty signature must classify as other");

    // Empty signature
    expect(common::classify_cpuid_signature("") ==
               common::hypervisor_type::unknown,
           "Empty signature must classify as unknown");
}

void test_ascii_case_folding() {
    namespace common = syscape::detail::virtualization_common;

    expect(common::case_insensitive_contains("MICROsoft Hv", "microsoft"),
           "ASCII containment must ignore letter case");
    expect(common::case_insensitive_equal("XeN", "xEn"),
           "ASCII equality must ignore letter case");
    expect(!common::case_insensitive_equal("Xen", "Xeo"),
           "ASCII equality must still distinguish different letters");
}

void test_sysfs_hypervisor_type_classification() {
    namespace backend = syscape::detail::virtualization_backend;
    namespace common = syscape::detail::virtualization_common;

    expect(backend::classify_sysfs_hypervisor_type("XEN") ==
               common::hypervisor_type::xen,
           "The Xen sysfs type must classify as Xen case-insensitively");
    expect(backend::classify_sysfs_hypervisor_type("custom-hypervisor") ==
               common::hypervisor_type::other,
           "A non-Xen sysfs type must classify as another present hypervisor");
    expect(backend::classify_sysfs_hypervisor_type("") ==
               common::hypervisor_type::unknown,
           "An empty sysfs type must not fabricate a vendor");
}

void test_dmi_classification() {
    namespace common = syscape::detail::virtualization_common;

    expect(common::classify_dmi_strings("QEMU", "Standard PC", "Bochs") ==
               common::hypervisor_type::qemu,
           "QEMU DMI strings must classify as QEMU");
    expect(common::classify_dmi_strings("innotek GmbH", "VirtualBox", "innotek") ==
               common::hypervisor_type::virtualbox,
           "VirtualBox DMI strings must classify as VirtualBox");
    expect(common::classify_dmi_strings("VMware, Inc.", "VMware Virtual Platform", "Phoenix") ==
               common::hypervisor_type::vmware,
           "VMware DMI strings must classify as VMware");
    expect(common::classify_dmi_strings("Microsoft Corporation", "Virtual Machine", "American Megatrends") ==
               common::hypervisor_type::hyper_v,
           "Microsoft VM DMI strings must classify as Hyper-V");
    expect(common::classify_dmi_strings("Xen", "HVM domU", "Xen") ==
               common::hypervisor_type::xen,
           "Xen DMI strings must classify as Xen");
    expect(common::classify_dmi_strings("Amazon EC2", "t3.medium", "Amazon EC2") ==
               common::hypervisor_type::kvm,
           "Amazon EC2 must classify as KVM-based cloud hypervisor");
    expect(common::classify_dmi_strings("Google", "Google Compute Engine", "Google") ==
               common::hypervisor_type::kvm,
           "Google Compute Engine must classify as KVM");
    expect(common::classify_dmi_strings("LENOVO", "20XX", "LENOVO") ==
               common::hypervisor_type::unknown,
           "Physical bare-metal DMI strings must return unknown");
}

void test_container_classification() {
    namespace common = syscape::detail::virtualization_common;

    expect(common::classify_container_name("docker") ==
               common::container_type::docker,
           "docker must classify as docker");
    expect(common::classify_container_name("podman") ==
               common::container_type::podman,
           "podman must classify as podman");
    expect(common::classify_container_name("lxc") ==
               common::container_type::lxc,
           "lxc must classify as lxc");
    expect(common::classify_container_name("lxd") ==
               common::container_type::lxd,
           "lxd must classify as lxd");
    expect(common::classify_container_name("containerd") ==
               common::container_type::containerd,
           "containerd must classify as containerd");
    expect(common::classify_container_name("systemd-nspawn") ==
               common::container_type::systemd_nspawn,
           "systemd-nspawn must classify as systemd_nspawn");
    expect(common::classify_container_name("wsl") ==
               common::container_type::wsl,
           "wsl must classify as wsl");
    expect(common::classify_container_name("custom-rt") ==
               common::container_type::other,
           "Custom container name must classify as other");
    expect(common::classify_container_name("") ==
               common::container_type::unknown,
           "Empty container name must classify as unknown");

    // cgroup line matching
    expect(common::classify_cgroup_line("12:memory:/docker/1234567890abcdef") ==
               common::container_type::docker,
           "cgroup docker line must classify as docker");
    expect(common::classify_cgroup_line("0::/system.slice/docker-1234.scope") ==
               common::container_type::docker,
           "cgroup v2 docker line must classify as docker");
    expect(common::classify_cgroup_line("0::/user.slice/user-1000.slice/libpod-123.scope") ==
               common::container_type::podman,
           "cgroup podman line must classify as podman");
    expect(common::classify_cgroup_line("1:name=systemd:/kubepods/burstable/pod123/456") ==
               common::container_type::kubernetes,
           "cgroup kubepods line must classify as kubernetes");
    expect(common::classify_cgroup_line("0::/lxc/my-container") ==
               common::container_type::lxc,
           "cgroup lxc line must classify as lxc");
    expect(common::classify_cgroup_line("0::/user.slice/user-1000.slice/session-1.scope") ==
               common::container_type::none,
           "Normal user cgroup line must classify as none");
}

void test_namespace_classification() {
    namespace common = syscape::detail::virtualization_common;

    expect(common::classify_namespace_name("cgroup") ==
               common::namespace_category::cgroup,
           "cgroup namespace must classify as cgroup");
    expect(common::classify_namespace_name("ipc") ==
               common::namespace_category::ipc,
           "ipc namespace must classify as ipc");
    expect(common::classify_namespace_name("mnt") ==
               common::namespace_category::mount,
           "mnt namespace must classify as mount");
    expect(common::classify_namespace_name("net") ==
               common::namespace_category::net,
           "net namespace must classify as net");
    expect(common::classify_namespace_name("pid") ==
               common::namespace_category::pid,
           "pid namespace must classify as pid");
    expect(common::classify_namespace_name("pid_for_children") ==
               common::namespace_category::pid_for_children,
           "pid_for_children namespace must classify as pid_for_children");
    expect(common::classify_namespace_name("time") ==
               common::namespace_category::time,
           "time namespace must classify as time");
    expect(common::classify_namespace_name("time_for_children") ==
               common::namespace_category::time_for_children,
           "time_for_children namespace must classify as time_for_children");
    expect(common::classify_namespace_name("user") ==
               common::namespace_category::user,
           "user namespace must classify as user");
    expect(common::classify_namespace_name("uts") ==
               common::namespace_category::uts,
           "uts namespace must classify as uts");
    expect(common::classify_namespace_name("unknown_ns") ==
               common::namespace_category::unknown,
           "unrecognized namespace must classify as unknown");
}

void test_namespace_link_parsing() {
    namespace common = syscape::detail::virtualization_common;

    std::string type_name;
    std::uint64_t inode = 0U;

    expect(common::parse_namespace_link("net:[4026531833]", type_name, inode),
           "Valid net namespace link must parse");
    expect(type_name == "net", "Type name must be net");
    expect(inode == 4026531833ULL, "Inode must match parsed number");

    expect(common::parse_namespace_link("pid:[123456]", type_name, inode),
           "Valid pid namespace link must parse");
    expect(type_name == "pid", "Type name must be pid");
    expect(inode == 123456ULL, "Inode must match 123456");

    expect(!common::parse_namespace_link("invalid_format", type_name, inode),
           "Invalid namespace format without bracket must fail");
    expect(!common::parse_namespace_link("net:[]", type_name, inode),
           "Empty bracket contents must fail");
    expect(!common::parse_namespace_link("net:[abc]", type_name, inode),
           "Non-numeric bracket contents must fail");
    expect(!common::parse_namespace_link("net:[4026531833]garbage", type_name, inode),
           "Trailing garbage after bracket must fail");
    expect(!common::parse_namespace_link("net:[4026531833] ", type_name, inode),
           "Trailing space after bracket must fail");
}

void test_cgroup_limit_parsing() {
    namespace common = syscape::detail::virtualization_common;

    // "max" -> nullopt (unlimited)
    auto res_max = common::parse_cgroup_limit_value("max");
    expect(res_max.has_value() && !res_max->has_value(),
           "max must convert to nullopt limit");

    // Empty string must be malformed_data
    auto res_empty = common::parse_cgroup_limit_value("   \n");
    expect(!res_empty && res_empty.error() == syscape::errc::malformed_data,
           "empty string must report malformed_data");

    // Negative (cgroup v1 -1) -> nullopt
    auto res_neg1 = common::parse_cgroup_limit_value("-1\n");
    expect(res_neg1.has_value() && !res_neg1->has_value(),
           "-1 must convert to nullopt limit");

    // Other negative numbers -> malformed_data
    auto res_neg2 = common::parse_cgroup_limit_value("-2\n");
    expect(!res_neg2 && res_neg2.error() == syscape::errc::malformed_data,
           "-2 must report malformed_data");

    auto res_neggarbage = common::parse_cgroup_limit_value("-garbage\n");
    expect(!res_neggarbage && res_neggarbage.error() == syscape::errc::malformed_data,
           "-garbage must report malformed_data");

    // Standard integer
    auto res_num = common::parse_cgroup_limit_value("104857600\n");
    expect(res_num.has_value() && res_num->has_value() && **res_num == 104857600ULL,
           "104857600 must parse to 104857600");

    // Sentinel check: PAGE_COUNTER_MAX (cgroup v1 unlimited)
    auto res_sentinel = common::parse_cgroup_limit_value("9223372036854771712\n");
    expect(res_sentinel.has_value() && !res_sentinel->has_value(),
           "PAGE_COUNTER_MAX must convert to nullopt limit");

    // Malformed string
    auto res_invalid = common::parse_cgroup_limit_value("1234xyz");
    expect(!res_invalid && res_invalid.error() == syscape::errc::malformed_data,
           "Non-numeric limit text must report malformed_data");
}

void test_cgroup_cpu_max_parsing() {
    namespace common = syscape::detail::virtualization_common;

    std::optional<std::uint64_t> quota;
    std::optional<std::uint64_t> period;

    auto res1 = common::parse_cgroup_cpu_max("max 100000\n", quota, period);
    expect(res1.has_value(), "parse max 100000 must succeed");
    expect(!quota.has_value(), "quota for max must be nullopt");
    expect(period.has_value() && *period == 100000ULL, "period must be 100000");

    auto res2 = common::parse_cgroup_cpu_max("50000 100000\n", quota, period);
    expect(res2.has_value(), "parse 50000 100000 must succeed");
    expect(quota.has_value() && *quota == 50000ULL, "quota must be 50000");
    expect(period.has_value() && *period == 100000ULL, "period must be 100000");

    // Single-field inputs are not valid cgroup v2 cpu.max formats and must be rejected
    auto res3 = common::parse_cgroup_cpu_max("max", quota, period);
    expect(!res3 && res3.error() == syscape::errc::malformed_data,
           "single token 'max' must report malformed_data");

    auto res4 = common::parse_cgroup_cpu_max("50000", quota, period);
    expect(!res4 && res4.error() == syscape::errc::malformed_data,
           "single token '50000' must report malformed_data");

    auto res_err_period_max = common::parse_cgroup_cpu_max("50000 max", quota, period);
    expect(!res_err_period_max && res_err_period_max.error() == syscape::errc::malformed_data,
           "period cannot be 'max'");

    auto res_err_period_zero = common::parse_cgroup_cpu_max("50000 0", quota, period);
    expect(!res_err_period_zero && res_err_period_zero.error() == syscape::errc::malformed_data,
           "period cannot be 0");

    auto res_err_quota_zero = common::parse_cgroup_cpu_max("0 100000", quota, period);
    expect(!res_err_quota_zero &&
               res_err_quota_zero.error() == syscape::errc::malformed_data,
           "quota cannot be 0");

    auto res_err1 = common::parse_cgroup_cpu_max("bad 100000", quota, period);
    expect(!res_err1 && res_err1.error() == syscape::errc::malformed_data,
           "malformed cpu.max must report malformed_data");

    auto res_err2 = common::parse_cgroup_cpu_max("50000 bad", quota, period);
    expect(!res_err2 && res_err2.error() == syscape::errc::malformed_data,
           "malformed period must report malformed_data");

    auto res_err3 = common::parse_cgroup_cpu_max("", quota, period);
    expect(!res_err3 && res_err3.error() == syscape::errc::malformed_data,
           "empty cpu.max must report malformed_data");
}

void test_cgroup1_cpu_quota_parsing() {
    namespace common = syscape::detail::virtualization_common;

    const auto unlimited = common::parse_cgroup1_cpu_quota_value("-1\n");
    expect(unlimited.has_value() && !unlimited->has_value(),
           "cgroup v1 CPU quota -1 must mean unlimited");

    const auto quota = common::parse_cgroup1_cpu_quota_value("50000\n");
    expect(quota.has_value() && quota->has_value() && **quota == 50000ULL,
           "positive cgroup v1 CPU quota must parse");

    for (const std::string_view invalid : {
             std::string_view("max"), std::string_view("0"),
             std::string_view("-2"), std::string_view("12x"),
             std::string_view("9223372036854775808"),
             std::string_view("18446744073709551616")}) {
        const auto parsed = common::parse_cgroup1_cpu_quota_value(invalid);
        expect(!parsed && parsed.error() == syscape::errc::malformed_data,
               "invalid cgroup v1 CPU quota must report malformed_data");
    }
}

void test_exact_ratio_comparison() {
    namespace common = syscape::detail::virtualization_common;

    for (std::uint64_t lhs_n = 1U; lhs_n <= 32U; ++lhs_n) {
        for (std::uint64_t lhs_d = 1U; lhs_d <= 32U; ++lhs_d) {
            for (std::uint64_t rhs_n = 1U; rhs_n <= 32U; ++rhs_n) {
                for (std::uint64_t rhs_d = 1U; rhs_d <= 32U; ++rhs_d) {
                    const bool expected = lhs_n * rhs_d < rhs_n * lhs_d;
                    expect(common::positive_ratio_less(
                               lhs_n, lhs_d, rhs_n, rhs_d) == expected,
                           "exact ratio comparison must match cross multiplication");
                }
            }
        }
    }

    expect(common::positive_ratio_less(1U, 2U, 2U, 3U),
           "1/2 must be less than 2/3");
    expect(!common::positive_ratio_less(2U, 3U, 1U, 2U),
           "2/3 must not be less than 1/2");
    expect(!common::positive_ratio_less(2U, 4U, 1U, 2U),
           "equal ratios must not compare less");
    expect(common::positive_ratio_less(9007199254740993ULL,
                                       9007199254740992ULL,
                                       9007199254740992ULL,
                                       9007199254740991ULL),
           "ratio comparison must retain distinctions beyond double precision");
    expect(!common::positive_ratio_less(9007199254740992ULL,
                                        9007199254740991ULL,
                                        9007199254740993ULL,
                                        9007199254740992ULL),
           "reversed large ratios must not compare less");
}

void test_uid_map_classification() {
    namespace common = syscape::detail::virtualization_common;

    expect(common::is_full_identity_uid_map("0          0 4294967295\n"),
           "kernel-formatted full UID identity map must be recognized");
    expect(common::is_full_identity_uid_map("  0\t0  4294967295  "),
           "UID identity map recognition must ignore whitespace formatting");
    expect(!common::is_full_identity_uid_map("0 1000 1\n"),
           "translated UID map must prove a nested user namespace");
    expect(!common::is_full_identity_uid_map(
               "0 0 1\n1 1 4294967294\n"),
           "multi-range UID map must not be treated as the initial namespace map");
}

void test_cgroup_controllers_splitting() {
    namespace common = syscape::detail::virtualization_common;

    auto list1 = common::split_cgroup_controllers("cpu memory pids io\n");
    expect(list1.size() == 4U, "Should split into 4 controllers");
    if (list1.size() == 4U) {
        expect(list1[0] == "cpu", "ctrl 0 must be cpu");
        expect(list1[1] == "memory", "ctrl 1 must be memory");
        expect(list1[2] == "pids", "ctrl 2 must be pids");
        expect(list1[3] == "io", "ctrl 3 must be io");
    }

    auto list2 = common::split_cgroup_controllers("cpuset,cpu,memory");
    expect(list2.size() == 3U, "Comma separated controllers must split");

    auto list3 = common::split_cgroup_controllers("   \n\t ");
    expect(list3.empty(), "Whitespace string must yield empty list");
}

void test_cgroup_proc_file_parsing() {
    namespace common = syscape::detail::virtualization_common;

    common::cgroup_version_type ver = common::cgroup_version_type::none;
    std::string path;

    // v2 unified
    common::parse_cgroup_proc_file("0::/user.slice/user-1000.slice\n", ver, path);
    expect(ver == common::cgroup_version_type::v2, "0:: line must parse as v2");
    expect(path == "/user.slice/user-1000.slice", "v2 path must match");

    // v1 legacy multi-hierarchy
    const auto detailed_res = common::parse_cgroup_proc_file_detailed(
        "12:memory:/docker/123\n11:cpu,cpuacct:/system.slice\n10:pids:/user.slice\n");
    expect(detailed_res.has_value(), "v1 cgroup detailed parsing must succeed");
    if (detailed_res) {
        const auto& detailed = *detailed_res;
        expect(detailed.version == common::cgroup_version_type::v1, "Legacy lines must parse as v1");
        expect(detailed.v1_hierarchies.size() == 3U, "Must parse 3 v1 hierarchies");
        if (detailed.v1_hierarchies.size() == 3U) {
            expect(detailed.v1_hierarchies[0].controllers.size() == 1U &&
                   detailed.v1_hierarchies[0].controllers[0] == "memory", "ctrl 0 must be memory");
            expect(detailed.v1_hierarchies[0].path == "/docker/123", "path 0 must be /docker/123");
            expect(detailed.v1_hierarchies[1].controllers.size() == 2U &&
                   detailed.v1_hierarchies[1].controllers[0] == "cpu", "ctrl 1 must contain cpu");
            expect(detailed.v1_hierarchies[1].path == "/system.slice", "path 1 must be /system.slice");
            expect(detailed.v1_hierarchies[2].controllers.size() == 1U &&
                   detailed.v1_hierarchies[2].controllers[0] == "pids", "ctrl 2 must be pids");
            expect(detailed.v1_hierarchies[2].path == "/user.slice", "path 2 must be /user.slice");
        }
    }

    // hybrid
    common::parse_cgroup_proc_file(
        "0::/app\n1:name=systemd:/app\n", ver, path);
    expect(ver == common::cgroup_version_type::hybrid, "Mixed lines must parse as hybrid");

    // Trailing space preservation in cgroup paths
    const auto space_res = common::parse_cgroup_proc_file_detailed("0::/user.slice/my test \r\n");
    expect(space_res.has_value(), "Path with trailing space must succeed");
    if (space_res) {
        expect(space_res->v2_path == "/user.slice/my test ", "Trailing spaces in path must be preserved");
    }

    // Malformed cases
    auto bad1 = common::parse_cgroup_proc_file_detailed("not_a_cgroup_file");
    expect(!bad1 && bad1.error() == syscape::errc::malformed_data,
           "cgroup line without colons must report malformed_data");

    auto bad2 = common::parse_cgroup_proc_file_detailed("abc::/path");
    expect(!bad2 && bad2.error() == syscape::errc::malformed_data,
           "non-numeric hierarchy id must report malformed_data");

    auto bad3 = common::parse_cgroup_proc_file_detailed("1::/path");
    expect(!bad3 && bad3.error() == syscape::errc::malformed_data,
           "v1 entry without controller names must report malformed_data");

    auto bad4 = common::parse_cgroup_proc_file_detailed("0:cpu:/path");
    expect(!bad4 && bad4.error() == syscape::errc::malformed_data,
           "v2 hierarchy 0 with non-empty controller must report malformed_data");

    auto bad_overflow = common::parse_cgroup_proc_file_detailed("999999999999999999999999999999::/path");
    expect(!bad_overflow && bad_overflow.error() == syscape::errc::malformed_data,
           "huge overflow hierarchy id must report malformed_data");
}

void test_cgroup_ancestor_splitting() {
    namespace common = syscape::detail::virtualization_common;

    // Test is_mount_root_prefix
    expect(common::is_mount_root_prefix("/", "/a/b/c"), "root / must be prefix of any path");
    expect(common::is_mount_root_prefix("/a/b", "/a/b/c"), "/a/b must be prefix of /a/b/c");
    expect(common::is_mount_root_prefix("/a/b/c", "/a/b/c"), "exact match must be prefix");
    expect(!common::is_mount_root_prefix("/a/bc", "/a/b/c"), "/a/bc is not a path prefix of /a/b/c");
    expect(!common::is_mount_root_prefix("/other", "/a/b/c"), "/other is not a prefix of /a/b/c");

    // Test unescape_mountinfo_path
    const std::string unescaped = common::unescape_mountinfo_path("/sys/fs/cgroup/my\\040group\\134sub");
    expect(unescaped == "/sys/fs/cgroup/my group\\sub", "octal escape decoding must match");

    // Test get_cgroup_ancestor_dirs with root mount
    const auto dirs1 = common::get_cgroup_ancestor_dirs("/sys/fs/cgroup", "/", "/user.slice/user-1000.slice");
    expect(dirs1.size() == 3U, "Standard hierarchy should yield 3 ancestor directories");
    if (dirs1.size() == 3U) {
        expect(dirs1[0] == "/sys/fs/cgroup/user.slice/user-1000.slice", "dir 0 must be leaf");
        expect(dirs1[1] == "/sys/fs/cgroup/user.slice", "dir 1 must be parent");
        expect(dirs1[2] == "/sys/fs/cgroup", "dir 2 must be root mount");
    }

    // Test get_cgroup_ancestor_dirs with bind-mounted subtree root
    const auto dirs2 = common::get_cgroup_ancestor_dirs("/sys/fs/cgroup", "/user.slice", "/user.slice/user-1000.slice");
    expect(dirs2.size() == 2U, "Bind-mounted subtree should yield 2 ancestor directories");
    if (dirs2.size() == 2U) {
        expect(dirs2[0] == "/sys/fs/cgroup/user-1000.slice", "dir 0 must be relative leaf");
        expect(dirs2[1] == "/sys/fs/cgroup", "dir 1 must be mount point");
    }

    // Test get_cgroup_ancestor_dirs with root matching
    const auto dirs3 = common::get_cgroup_ancestor_dirs("/sys/fs/cgroup", "/", "/");
    expect(dirs3.size() == 1U && dirs3[0] == "/sys/fs/cgroup", "Root cgroup must yield 1 ancestor");
}

void test_live_queries() {
    const auto hv_present = syscape::virtualization::is_hypervisor_present();
    expect(hv_present.has_value(), "is_hypervisor_present must succeed");
    if (hv_present) {
        const auto hv_vendor = syscape::virtualization::hypervisor();
        expect(hv_vendor.has_value(), "hypervisor query must succeed");
        if (*hv_present) {
            expect(*hv_vendor != syscape::virtualization::hypervisor_vendor::none,
                   "If hypervisor is present, vendor must not be none");
            const auto name = syscape::virtualization::hypervisor_name();
            expect(name.has_value() && !name->empty(),
                   "If hypervisor is present, hypervisor_name should return non-empty name");
        } else {
            expect(*hv_vendor == syscape::virtualization::hypervisor_vendor::none,
                   "If hypervisor is not present, vendor must be none");
            const auto name = syscape::virtualization::hypervisor_name();
            expect(!name && name.error() == syscape::errc::not_found,
                   "If hypervisor is not present, hypervisor_name must return not_found");
        }
    }

    const auto cont_present = syscape::virtualization::is_container();
    expect(cont_present.has_value(), "is_container must succeed");
    if (cont_present) {
        const auto cont_rt = syscape::virtualization::container();
        expect(cont_rt.has_value(), "container query must succeed");
        if (*cont_present) {
            expect(*cont_rt != syscape::virtualization::container_runtime::none,
                   "If container is present, runtime must not be none");
            const auto name = syscape::virtualization::container_name();
            expect(name.has_value() && !name->empty(),
                   "If container is present, container_name should return name");
        } else {
            expect(*cont_rt == syscape::virtualization::container_runtime::none,
                   "If container is not present, runtime must be none");
            const auto name = syscape::virtualization::container_name();
            expect(!name && name.error() == syscape::errc::not_found,
                   "If not in container, container_name must return not_found");
        }
    }

    const auto wsl_check = syscape::virtualization::is_wsl();
    expect(wsl_check.has_value(), "is_wsl must succeed");
    if (wsl_check) {
        const auto ver = syscape::virtualization::wsl_version();
        if (*wsl_check) {
            expect(ver.has_value() && (*ver == 1U || *ver == 2U),
                   "If running in WSL, wsl_version must be 1 or 2");
        } else {
            expect(!ver && ver.error() == syscape::errc::not_found,
                   "If not in WSL, wsl_version must report not_found");
        }
    }

    const auto sandboxed = syscape::virtualization::is_sandboxed();
    expect(sandboxed.has_value(), "is_sandboxed must succeed");
    if (sandboxed) {
        const auto sb_type = syscape::virtualization::sandbox();
        expect(sb_type.has_value(), "sandbox query must succeed");
        if (*sandboxed) {
            expect(*sb_type != syscape::virtualization::sandbox_type::none,
                   "If sandboxed, sandbox_type must not be none");
        } else {
            expect(*sb_type == syscape::virtualization::sandbox_type::none,
                   "If not sandboxed, sandbox_type must be none");
        }
    }

    // Live cgroup queries
    const auto cg_ver = syscape::virtualization::cgroup_hierarchy_version();
    expect(cg_ver.has_value(), "cgroup_hierarchy_version must succeed on Linux");
    if (cg_ver) {
        expect(*cg_ver == syscape::virtualization::cgroup_version::v2 ||
               *cg_ver == syscape::virtualization::cgroup_version::v1 ||
               *cg_ver == syscape::virtualization::cgroup_version::hybrid,
               "Linux host cgroup version should be v1, v2, or hybrid");
    }

    const auto cg_info = syscape::virtualization::current_cgroup();
    expect(cg_info.has_value(), "current_cgroup must succeed on Linux");
    if (cg_info) {
        expect(!cg_info->path.empty(), "cgroup path must not be empty");
    }

    // Live namespace queries
    const auto ns_list = syscape::virtualization::namespaces();
    expect(ns_list.has_value(), "namespaces must succeed on Linux");
    if (ns_list) {
        expect(!ns_list->empty(), "namespaces list should not be empty on modern Linux");
        for (const auto& ns : *ns_list) {
            expect(!ns.name.empty(), "namespace name must not be empty");
            expect(ns.inode > 0U, "namespace inode must be greater than zero");
            expect(ns.type != syscape::virtualization::namespace_type::unknown,
                   "standard Linux namespaces should have known classification");
        }
    }

    const auto ns_iso = syscape::virtualization::is_namespace_isolated();
    expect(ns_iso.has_value() || ns_iso.error() == syscape::errc::permission_denied,
           "is_namespace_isolated must succeed or report permission_denied");
}

} // namespace

int main() {
    test_cpuid_signature_decoding();
    test_ascii_case_folding();
    test_sysfs_hypervisor_type_classification();
    test_dmi_classification();
    test_container_classification();
    test_namespace_classification();
    test_namespace_link_parsing();
    test_cgroup_limit_parsing();
    test_cgroup_cpu_max_parsing();
    test_cgroup1_cpu_quota_parsing();
    test_exact_ratio_comparison();
    test_uid_map_classification();
    test_cgroup_controllers_splitting();
    test_cgroup_proc_file_parsing();
    test_cgroup_ancestor_splitting();
    test_live_queries();
    return failures == 0 ? 0 : 1;
}
