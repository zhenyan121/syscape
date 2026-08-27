#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_LINUX_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <dirent.h>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <tuple>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/virtualization/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace virtualization_backend {

struct hypervisor_info {
    bool present = false;
    virtualization_common::hypervisor_type vendor =
        virtualization_common::hypervisor_type::none;
    std::string name;
};

struct container_info {
    bool present = false;
    virtualization_common::container_type runtime =
        virtualization_common::container_type::none;
    std::string name;
};

struct wsl_info {
    bool is_wsl = false;
    std::uint32_t version = 0U;
};

struct sandbox_info {
    bool is_sandboxed = false;
    virtualization_common::sandbox_type type =
        virtualization_common::sandbox_type::none;
};

/// Probes x86 / x86_64 CPUID leaves for hypervisor presence and signature.
inline hypervisor_info probe_cpuid_hypervisor() noexcept {
    hypervisor_info info;
#if defined(__i386__) || defined(__x86_64__)
    unsigned int eax = 0U;
    unsigned int ebx = 0U;
    unsigned int ecx = 0U;
    unsigned int edx = 0U;

    if (__get_cpuid(1U, &eax, &ebx, &ecx, &edx) != 0) {
        constexpr unsigned int hypervisor_bit = 1U << 31U;
        if ((ecx & hypervisor_bit) != 0U) {
            info.present = true;
            if (__get_cpuid(0x40000000U, &eax, &ebx, &ecx, &edx) != 0) {
                info.name = virtualization_common::decode_cpuid_signature(
                    ebx, ecx, edx);
                info.vendor =
                    virtualization_common::classify_cpuid_signature(info.name);
            } else {
                info.vendor = virtualization_common::hypervisor_type::unknown;
            }
        }
    }
#endif
    return info;
}

/// Reads one sysfs DMI attribute, returning empty string on absence.
inline std::string read_dmi_attribute(const char* name) {
    const std::string path = std::string("/sys/class/dmi/id/") + name;
    const result<std::string> content =
        linux_platform::read_text_file(path.c_str(), 1024U);
    if (!content) { return std::string(); }
    std::string value = *content;
    linux_platform::trim_line_end(value);
    return value;
}

/// Classifies the Linux sysfs hypervisor type token.
inline virtualization_common::hypervisor_type classify_sysfs_hypervisor_type(
    std::string_view type_name) noexcept {
    if (type_name.empty()) {
        return virtualization_common::hypervisor_type::unknown;
    }
    if (virtualization_common::case_insensitive_equal(type_name, "xen")) {
        return virtualization_common::hypervisor_type::xen;
    }
    return virtualization_common::hypervisor_type::other;
}

/// Detects hypervisor from CPUID, DMI attributes, and kernel sysfs.
inline result<hypervisor_info> detect_hypervisor() {
    hypervisor_info info = probe_cpuid_hypervisor();
    if (info.present &&
        info.vendor != virtualization_common::hypervisor_type::unknown &&
        info.vendor != virtualization_common::hypervisor_type::none &&
        info.vendor != virtualization_common::hypervisor_type::other) {
        return info;
    }

    // Check /sys/hypervisor/type
    const result<std::string> hypervisor_type_file =
        linux_platform::read_text_file("/sys/hypervisor/type", 256U);
    if (hypervisor_type_file) {
        std::string type_name = *hypervisor_type_file;
        linux_platform::trim_line_end(type_name);
        if (!type_name.empty()) {
            info.present = true;
            if (info.name.empty()) { info.name = type_name; }
            info.vendor = classify_sysfs_hypervisor_type(type_name);
            return info;
        }
    }

    // Check DMI vendor / product / bios strings
    const std::string sys_vendor = read_dmi_attribute("sys_vendor");
    const std::string product_name = read_dmi_attribute("product_name");
    const std::string bios_vendor = read_dmi_attribute("bios_vendor");

    const virtualization_common::hypervisor_type dmi_vendor =
        virtualization_common::classify_dmi_strings(sys_vendor, product_name,
                                                    bios_vendor);
    if (dmi_vendor != virtualization_common::hypervisor_type::unknown) {
        info.present = true;
        info.vendor = dmi_vendor;
        if (info.name.empty()) {
            info.name = !product_name.empty() ? product_name : sys_vendor;
        }
        return info;
    }

    // Check Device Tree for ARM/RISC-V virtualization
    if (::access("/proc/device-tree/hypervisor", F_OK) == 0) {
        info.present = true;
        if (info.vendor == virtualization_common::hypervisor_type::none) {
            info.vendor = virtualization_common::hypervisor_type::unknown;
        }
        if (info.name.empty()) { info.name = "DeviceTree Hypervisor"; }
        return info;
    }

    return info;
}

/// Detects WSL execution and major version (1 or 2).
inline result<wsl_info> detect_wsl() {
    wsl_info info;
    bool wsl_hint = false;

    if (::access("/proc/sys/fs/binfmt_misc/WSLInterop", F_OK) == 0 ||
        ::access("/run/WSL", F_OK) == 0) {
        wsl_hint = true;
    }

    const result<std::string> version_content =
        linux_platform::read_text_file("/proc/version", 4096U);
    if (version_content) {
        if (virtualization_common::case_insensitive_contains(*version_content,
                                                             "microsoft") ||
            virtualization_common::case_insensitive_contains(*version_content,
                                                             "wsl")) {
            wsl_hint = true;
        }
    }

    if (!wsl_hint) {
        return info;
    }

    info.is_wsl = true;
    info.version = 1U;

    // Check for WSL 2 indicators: "microsoft-standard-WSL2" or /dev/dxg
    if (version_content) {
        if (virtualization_common::case_insensitive_contains(
                *version_content, "wsl2") ||
            virtualization_common::case_insensitive_contains(
                *version_content, "microsoft-standard-wsl2")) {
            info.version = 2U;
            return info;
        }
    }

    const result<std::string> osrelease_content =
        linux_platform::read_text_file("/proc/sys/kernel/osrelease", 1024U);
    if (osrelease_content) {
        if (virtualization_common::case_insensitive_contains(
                *osrelease_content, "wsl2") ||
            virtualization_common::case_insensitive_contains(
                *osrelease_content, "microsoft-standard-wsl2")) {
            info.version = 2U;
            return info;
        }
    }

    if (::access("/dev/dxg", F_OK) == 0) {
        info.version = 2U;
        return info;
    }

    return info;
}

/// Detects container runtime from systemd container file, cgroups, and known paths.
inline result<container_info> detect_container() {
    container_info info;

    // 1. Check /run/systemd/container
    const result<std::string> systemd_container =
        linux_platform::read_text_file("/run/systemd/container", 256U);
    if (systemd_container) {
        std::string name = *systemd_container;
        linux_platform::trim_line_end(name);
        if (!name.empty()) {
            info.present = true;
            info.name = name;
            info.runtime =
                virtualization_common::classify_container_name(name);
            return info;
        }
    }

    // 2. Check /.dockerenv
    if (::access("/.dockerenv", F_OK) == 0) {
        info.present = true;
        info.runtime = virtualization_common::container_type::docker;
        info.name = "docker";
        return info;
    }

    // 3. Check /.containerenv
    if (::access("/.containerenv", F_OK) == 0) {
        info.present = true;
        info.runtime = virtualization_common::container_type::podman;
        info.name = "podman";
        return info;
    }

    // 4. Check /proc/vz for OpenVZ
    if (::access("/proc/vz", F_OK) == 0 && ::access("/proc/bc", F_OK) != 0) {
        info.present = true;
        info.runtime = virtualization_common::container_type::openvz;
        info.name = "openvz";
        return info;
    }

    // 5. Inspect /proc/1/cgroup and /proc/self/cgroup
    const auto check_cgroup_file = [](const char* path,
                                      container_info& target) -> bool {
        const result<std::string> cgroup_content =
            linux_platform::read_text_file(path, 16U * 1024U);
        if (!cgroup_content) { return false; }
        const std::string_view content_view = *cgroup_content;
        std::size_t offset = 0U;
        while (offset < content_view.size()) {
            const std::size_t end = content_view.find('\n', offset);
            const std::string_view line = content_view.substr(
                offset, end == std::string_view::npos
                            ? content_view.size() - offset
                            : end - offset);
            offset = end == std::string_view::npos ? content_view.size()
                                                   : end + 1U;

            const virtualization_common::container_type classified =
                virtualization_common::classify_cgroup_line(line);
            if (classified != virtualization_common::container_type::none) {
                target.present = true;
                target.runtime = classified;
                switch (classified) {
                case virtualization_common::container_type::docker:
                    target.name = "docker";
                    break;
                case virtualization_common::container_type::podman:
                    target.name = "podman";
                    break;
                case virtualization_common::container_type::kubernetes:
                    target.name = "kubernetes";
                    break;
                case virtualization_common::container_type::lxc:
                    target.name = "lxc";
                    break;
                case virtualization_common::container_type::lxd:
                    target.name = "lxd";
                    break;
                case virtualization_common::container_type::containerd:
                    target.name = "containerd";
                    break;
                default:
                    target.name = "container";
                    break;
                }
                return true;
            }
        }
        return false;
    };

    if (check_cgroup_file("/proc/1/cgroup", info) ||
        check_cgroup_file("/proc/self/cgroup", info)) {
        return info;
    }

    // 6. Check WSL
    const result<wsl_info> wsl = detect_wsl();
    if (wsl && wsl->is_wsl) {
        info.present = true;
        info.runtime = virtualization_common::container_type::wsl;
        info.name = "wsl";
        return info;
    }

    return info;
}

/// Detects application sandbox mechanisms (Flatpak, Snap).
inline result<sandbox_info> detect_sandbox() {
    sandbox_info info;

    if (::access("/.flatpak-info", F_OK) == 0) {
        info.is_sandboxed = true;
        info.type = virtualization_common::sandbox_type::flatpak;
        return info;
    }

    if (::getenv("SNAP") != nullptr || ::getenv("SNAP_NAME") != nullptr ||
        ::getenv("SNAP_INSTANCE_NAME") != nullptr) {
        info.is_sandboxed = true;
        info.type = virtualization_common::sandbox_type::snap;
        return info;
    }

    return info;
}

/// Splits a cgroup relative path into ancestor paths from leaf to root.
inline std::vector<std::string> get_ancestor_paths(std::string_view path) {
    std::vector<std::string> paths;
    if (path.empty() || path == "/") {
        paths.emplace_back("/");
        return paths;
    }

    std::string current(path);
    while (current.size() > 1U && current.back() == '/') {
        current.pop_back();
    }

    while (!current.empty() && current != "/") {
        paths.push_back(current);
        const std::size_t last_slash = current.rfind('/');
        if (last_slash == std::string::npos || last_slash == 0U) {
            break;
        }
        current = current.substr(0U, last_slash);
    }
    paths.emplace_back("/");
    return paths;
}

/// Reads and computes the effective hierarchical cgroup limit across all ancestor directories.
inline result<std::optional<std::uint64_t>> read_effective_cgroup_limit(
    const std::vector<std::string>& ancestor_dirs,
    const char* filename) {
    std::optional<std::uint64_t> min_limit;
    bool found_any_file = false;

    for (const auto& dir : ancestor_dirs) {
        std::string full_path = dir;
        if (full_path.empty() || full_path.back() != '/') {
            full_path += '/';
        }
        full_path += filename;

        struct stat st{};
        if (::stat(full_path.c_str(), &st) != 0) {
            if (errno == ENOENT) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }

        found_any_file = true;

        const result<std::string> content =
            linux_platform::read_text_file(full_path.c_str(), 256U);
        if (!content) {
            return fail(content.error());
        }

        const auto parsed = virtualization_common::parse_cgroup_limit_value(*content);
        if (!parsed) {
            return fail(parsed.error());
        }

        if (*parsed) {
            if (min_limit) {
                min_limit = std::min(*min_limit, **parsed);
            } else {
                min_limit = **parsed;
            }
        }
    }

    if (!found_any_file) {
        const std::string_view name(filename);
        if (name == "memory.swap.max" || name == "memory.high") {
            return std::optional<std::uint64_t>{};
        }
        return fail(errc::not_found);
    }

    return min_limit;
}

/// Computes the effective hierarchical CPU quota and period across all ancestor directories.
inline result<void> read_effective_cpu_max(
    const std::vector<std::string>& ancestor_dirs,
    std::optional<std::uint64_t>& effective_quota_us,
    std::optional<std::uint64_t>& effective_period_us) {
    effective_quota_us = std::nullopt;
    effective_period_us = std::nullopt;
    bool found_any_file = false;

    for (const auto& dir : ancestor_dirs) {
        std::string full_path = dir;
        if (full_path.empty() || full_path.back() != '/') {
            full_path += '/';
        }
        full_path += "cpu.max";

        struct stat st{};
        if (::stat(full_path.c_str(), &st) != 0) {
            if (errno == ENOENT) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }

        found_any_file = true;

        const result<std::string> content =
            linux_platform::read_text_file(full_path.c_str(), 256U);
        if (!content) {
            return fail(content.error());
        }

        std::optional<std::uint64_t> level_quota;
        std::optional<std::uint64_t> level_period;
        const auto parsed = virtualization_common::parse_cgroup_cpu_max(
            *content, level_quota, level_period);
        if (!parsed) {
            return fail(parsed.error());
        }

        if (level_period) {
            if (!effective_period_us) {
                effective_period_us = level_period;
            }
        }

        if (level_quota) {
            const std::uint64_t q = *level_quota;
            const std::uint64_t p = level_period.value_or(100000ULL);
            if (!effective_quota_us) {
                effective_quota_us = q;
                effective_period_us = p;
            } else {
                const std::uint64_t cur_q = *effective_quota_us;
                const std::uint64_t cur_p = effective_period_us.value_or(100000ULL);
                if (virtualization_common::positive_ratio_less(
                        q, p, cur_q, cur_p)) {
                    effective_quota_us = q;
                    effective_period_us = p;
                }
            }
        }
    }

    if (!found_any_file) {
        return fail(errc::not_found);
    }
    return {};
}

/// Reads and computes the effective hierarchical CPU quota and period across all ancestor directories for cgroup v1.
inline result<void> read_effective_cgroup1_cpu(
    const std::vector<std::string>& ancestor_dirs,
    std::optional<std::uint64_t>& effective_quota_us,
    std::optional<std::uint64_t>& effective_period_us) {
    effective_quota_us = std::nullopt;
    effective_period_us = std::nullopt;

    bool found_any_file = false;
    bool found_constrained = false;
    std::uint64_t best_quota = 0U;
    std::uint64_t best_period = 0U;
    std::optional<std::uint64_t> first_period;

    for (const auto& dir : ancestor_dirs) {
        std::string quota_path = dir;
        if (quota_path.empty() || quota_path.back() != '/') {
            quota_path += '/';
        }
        quota_path += "cpu.cfs_quota_us";

        std::string period_path = dir;
        if (period_path.empty() || period_path.back() != '/') {
            period_path += '/';
        }
        period_path += "cpu.cfs_period_us";

        struct stat st_q{};
        if (::stat(quota_path.c_str(), &st_q) != 0) {
            if (errno == ENOENT) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }

        found_any_file = true;

        const auto q_content = linux_platform::read_text_file(quota_path.c_str(), 256U);
        if (!q_content) {
            return fail(q_content.error());
        }
        const auto q_parsed =
            virtualization_common::parse_cgroup1_cpu_quota_value(*q_content);
        if (!q_parsed) {
            return fail(q_parsed.error());
        }

        struct stat st_p{};
        if (::stat(period_path.c_str(), &st_p) != 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        const auto p_content = linux_platform::read_text_file(period_path.c_str(), 256U);
        if (!p_content) {
            return fail(p_content.error());
        }
        const std::string_view p_str = virtualization_common::trim_signature(*p_content);
        if (p_str.empty()) {
            return fail(errc::malformed_data);
        }
        std::uint64_t p_val = 0U;
        for (char c : p_str) {
            if (c < '0' || c > '9') {
                return fail(errc::malformed_data);
            }
            const auto digit = static_cast<std::uint64_t>(c - '0');
            if (p_val > (UINT64_MAX - digit) / 10U) {
                return fail(errc::malformed_data);
            }
            p_val = p_val * 10U + digit;
        }
        if (p_val == 0U) {
            return fail(errc::malformed_data);
        }

        if (!first_period) {
            first_period = p_val;
        }

        if (*q_parsed) {
            const std::uint64_t q = **q_parsed;
            const std::uint64_t p = p_val;
            if (!found_constrained ||
                virtualization_common::positive_ratio_less(
                    q, p, best_quota, best_period)) {
                found_constrained = true;
                best_quota = q;
                best_period = p;
            }
        }
    }

    if (!found_any_file) {
        return fail(errc::not_found);
    }

    if (found_constrained) {
        effective_quota_us = best_quota;
        effective_period_us = best_period;
    } else {
        effective_quota_us = std::nullopt;
        effective_period_us = first_period;
    }
    return {};
}

/// Represents a parsed mount entry for cgroup filesystems.
struct cgroup_mount_info {
    std::string root;
    std::string mount_point;
    std::string fstype;
    std::vector<std::string> options;
};

/// Reads and parses all cgroup mounts from /proc/self/mountinfo (or fallback to /proc/mounts).
inline result<std::vector<cgroup_mount_info>> read_cgroup_mounts() {
    result<std::string> content =
        linux_platform::read_text_file("/proc/self/mountinfo", 2U * 1024U * 1024U);
    if (!content) {
        if (content.error() != std::errc::no_such_file_or_directory) {
            return fail(content.error());
        }
        content = linux_platform::read_text_file("/proc/mounts", 2U * 1024U * 1024U);
        if (!content) {
            return fail(content.error());
        }
        std::vector<cgroup_mount_info> mounts;
        const std::string_view text = *content;
        std::size_t offset = 0U;
        while (offset < text.size()) {
            const std::size_t end = text.find('\n', offset);
            const std::string_view line = text.substr(
                offset, end == std::string_view::npos ? text.size() - offset : end - offset);
            offset = end == std::string_view::npos ? text.size() : end + 1U;

            const auto tokens = virtualization_common::split_whitespace(line);
            if (tokens.size() >= 4U && (tokens[2] == "cgroup" || tokens[2] == "cgroup2")) {
                cgroup_mount_info entry;
                entry.root = "/";
                entry.mount_point = virtualization_common::unescape_mountinfo_path(tokens[1]);
                entry.fstype = std::string(tokens[2]);
                entry.options = virtualization_common::split_commas(tokens[3]);
                mounts.push_back(std::move(entry));
            }
        }
        return mounts;
    }

    std::vector<cgroup_mount_info> mounts;
    const std::string_view text = *content;
    std::size_t offset = 0U;
    while (offset < text.size()) {
        const std::size_t end = text.find('\n', offset);
        const std::string_view line = text.substr(
            offset, end == std::string_view::npos ? text.size() - offset : end - offset);
        offset = end == std::string_view::npos ? text.size() : end + 1U;

        const auto tokens = virtualization_common::split_whitespace(line);
        std::size_t sep_idx = 0U;
        for (std::size_t i = 0U; i < tokens.size(); ++i) {
            if (tokens[i] == "-") {
                sep_idx = i;
                break;
            }
        }
        if (sep_idx >= 6U && tokens.size() >= sep_idx + 4U) {
            const std::string_view fstype = tokens[sep_idx + 1U];
            if (fstype == "cgroup" || fstype == "cgroup2") {
                cgroup_mount_info entry;
                entry.root = virtualization_common::unescape_mountinfo_path(tokens[3]);
                entry.mount_point = virtualization_common::unescape_mountinfo_path(tokens[4]);
                entry.fstype = std::string(fstype);
                entry.options = virtualization_common::split_commas(tokens[sep_idx + 3U]);
                mounts.push_back(std::move(entry));
            }
        }
    }
    return mounts;
}

/// Discovers the most specific cgroup v2 mount point whose root covers proc_cgroup_path.
inline bool find_cgroup2_mount(
    const std::vector<cgroup_mount_info>& mounts,
    std::string_view proc_cgroup_path,
    std::string& mount_point,
    std::string& mount_root) {
    bool found = false;
    std::size_t longest_root_len = 0U;

    for (const auto& m : mounts) {
        if (m.fstype == "cgroup2") {
            if (virtualization_common::is_mount_root_prefix(m.root, proc_cgroup_path)) {
                if (!found || m.root.size() > longest_root_len) {
                    found = true;
                    longest_root_len = m.root.size();
                    mount_point = m.mount_point;
                    mount_root = m.root;
                }
            }
        }
    }

    if (found) {
        return true;
    }

    if (mounts.empty()) {
        struct statfs fs_stat{};
        if (::statfs("/sys/fs/cgroup", &fs_stat) == 0) {
            constexpr unsigned long cgroup2_magic = 0x63677270UL;
            if (static_cast<unsigned long>(fs_stat.f_type) == cgroup2_magic) {
                mount_point = "/sys/fs/cgroup";
                mount_root = "/";
                return true;
            }
        }
    }
    return false;
}

/// Discovers the most specific cgroup v1 controller mount point whose root covers proc_cgroup_path.
inline bool find_cgroup1_controller_mount(
    const std::vector<cgroup_mount_info>& mounts,
    std::string_view controller_name,
    std::string_view proc_cgroup_path,
    std::string& mount_point,
    std::string& mount_root) {
    bool found = false;
    std::size_t longest_root_len = 0U;

    for (const auto& m : mounts) {
        if (m.fstype == "cgroup") {
            for (const auto& opt : m.options) {
                if (opt == controller_name) {
                    if (virtualization_common::is_mount_root_prefix(m.root, proc_cgroup_path)) {
                        if (!found || m.root.size() > longest_root_len) {
                            found = true;
                            longest_root_len = m.root.size();
                            mount_point = m.mount_point;
                            mount_root = m.root;
                        }
                    }
                    break;
                }
            }
        }
    }

    if (found) {
        return true;
    }

    if (mounts.empty()) {
        const std::string candidate = "/sys/fs/cgroup/" + std::string(controller_name);
        struct stat st{};
        if (::stat(candidate.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            mount_point = candidate;
            mount_root = "/";
            return true;
        }
    }
    return false;
}

/// Collects all enabled controllers for the specific cgroup directory.
inline result<std::vector<std::string>> read_cgroup2_controllers(
    const std::string& target_dir) {
    const std::string ctrl_file = target_dir + "/cgroup.controllers";
    struct stat st{};
    if (::stat(ctrl_file.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    const auto ctrl_content = linux_platform::read_text_file(ctrl_file.c_str(), 1024U);
    if (!ctrl_content) {
        return fail(ctrl_content.error());
    }

    const auto raw_tokens = virtualization_common::split_whitespace(*ctrl_content);
    std::vector<std::string> result_list;
    result_list.reserve(raw_tokens.size());
    for (const auto& tok : raw_tokens) {
        result_list.emplace_back(tok);
    }
    std::sort(result_list.begin(), result_list.end());
    return result_list;
}

/// Detects the active cgroup hierarchy version on Linux.
inline result<virtualization_common::cgroup_version_type> detect_cgroup_version() {
    const result<std::string> cgroup_content =
        linux_platform::read_text_file("/proc/self/cgroup", 16U * 1024U);
    if (!cgroup_content) {
        return fail(cgroup_content.error());
    }

    const auto parsed_info =
        virtualization_common::parse_cgroup_proc_file_detailed(*cgroup_content);
    if (!parsed_info) {
        return fail(parsed_info.error());
    }

    if (parsed_info->version != virtualization_common::cgroup_version_type::none) {
        return parsed_info->version;
    }

    struct statfs fs_stat{};
    if (::statfs("/sys/fs/cgroup", &fs_stat) == 0) {
        constexpr unsigned long cgroup2_magic = 0x63677270UL;
        constexpr unsigned long cgroup1_magic = 0x27e0ebUL;
        const auto f_type = static_cast<unsigned long>(fs_stat.f_type);
        if (f_type == cgroup2_magic) {
            return virtualization_common::cgroup_version_type::v2;
        }
        if (f_type == cgroup1_magic) {
            return virtualization_common::cgroup_version_type::v1;
        }
    }

    return virtualization_common::cgroup_version_type::none;
}

/// Detects the current process cgroup path, controllers, and effective resource limits.
inline result<virtualization_common::cgroup_record> detect_current_cgroup() {
    const result<std::string> cgroup_content =
        linux_platform::read_text_file("/proc/self/cgroup", 16U * 1024U);
    if (!cgroup_content) {
        return fail(cgroup_content.error());
    }

    const auto parsed_info =
        virtualization_common::parse_cgroup_proc_file_detailed(*cgroup_content);
    if (!parsed_info) {
        return fail(parsed_info.error());
    }
    if (parsed_info->version == virtualization_common::cgroup_version_type::none) {
        return virtualization_common::cgroup_record{};
    }

    const auto mounts_res = read_cgroup_mounts();
    if (!mounts_res) {
        return fail(mounts_res.error());
    }
    const auto& mounts = *mounts_res;

    virtualization_common::cgroup_record record;
    record.version = parsed_info->version;

    // 1. Process cgroup v2 hierarchy (v2 or hybrid)
    if (record.version == virtualization_common::cgroup_version_type::v2 ||
        record.version == virtualization_common::cgroup_version_type::hybrid) {
        record.path = !parsed_info->v2_path.empty() ? parsed_info->v2_path : "/";
        if (record.path.empty()) {
            record.path = "/";
        }

        std::string v2_mount;
        std::string v2_root;
        if (!find_cgroup2_mount(mounts, record.path, v2_mount, v2_root)) {
            return fail(errc::not_found);
        }

        const auto ancestor_dirs = virtualization_common::get_cgroup_ancestor_dirs(
            v2_mount, v2_root, record.path);

        if (!ancestor_dirs.empty()) {
            auto ctrls_res = read_cgroup2_controllers(ancestor_dirs.front());
            if (!ctrls_res) {
                return fail(ctrls_res.error());
            }
            record.controllers = std::move(*ctrls_res);

            const auto has_v2_controller = [&](std::string_view name) {
                return std::find(record.controllers.begin(),
                                 record.controllers.end(), name) !=
                       record.controllers.end();
            };

            if (has_v2_controller("memory")) {
                auto mem_max_res = read_effective_cgroup_limit(
                    ancestor_dirs, "memory.max");
                if (!mem_max_res) { return fail(mem_max_res.error()); }
                record.limits.memory_max_bytes = *mem_max_res;

                auto mem_high_res = read_effective_cgroup_limit(
                    ancestor_dirs, "memory.high");
                if (!mem_high_res) { return fail(mem_high_res.error()); }
                record.limits.memory_high_bytes = *mem_high_res;

                auto mem_swap_res = read_effective_cgroup_limit(
                    ancestor_dirs, "memory.swap.max");
                if (!mem_swap_res) { return fail(mem_swap_res.error()); }
                record.limits.memory_swap_max_bytes = *mem_swap_res;
            }

            if (has_v2_controller("cpu")) {
                auto cpu_max_res = read_effective_cpu_max(
                    ancestor_dirs, record.limits.cpu_quota_us,
                    record.limits.cpu_period_us);
                if (!cpu_max_res) { return fail(cpu_max_res.error()); }
            }

            if (has_v2_controller("pids")) {
                auto pids_max_res = read_effective_cgroup_limit(
                    ancestor_dirs, "pids.max");
                if (!pids_max_res) { return fail(pids_max_res.error()); }
                record.limits.pids_max = *pids_max_res;
            }
        }
    }

    // 2. Process cgroup v1 hierarchy (v1 or hybrid)
    if (record.version == virtualization_common::cgroup_version_type::v1 ||
        record.version == virtualization_common::cgroup_version_type::hybrid) {
        for (const auto& hier : parsed_info->v1_hierarchies) {
            for (const auto& ctrl : hier.controllers) {
                if (ctrl.empty() || ctrl.compare(0, 5, "name=") == 0) {
                    // Skip non-resource named hierarchies like name=systemd
                    continue;
                }
                if (std::find(record.controllers.begin(),
                              record.controllers.end(), ctrl) == record.controllers.end()) {
                    record.controllers.push_back(ctrl);
                }
            }
        }
        std::sort(record.controllers.begin(), record.controllers.end());

        if (record.path.empty() || record.path == "/") {
            if (!parsed_info->v1_hierarchies.empty()) {
                record.path = parsed_info->v1_hierarchies.front().path;
            } else {
                record.path = "/";
            }
        }

        auto get_v1_controller_info = [&](std::string_view name) -> std::pair<bool, std::string> {
            for (const auto& hier : parsed_info->v1_hierarchies) {
                for (const auto& ctrl : hier.controllers) {
                    if (ctrl == name) {
                        return {true, hier.path};
                    }
                }
            }
            return {false, "/"};
        };

        // Memory (if not already set by v2)
        if (!record.limits.memory_max_bytes.has_value()) {
            const auto [has_mem, mem_rel] = get_v1_controller_info("memory");
            if (has_mem) {
                std::string mem_mount;
                std::string mem_root;
                if (!find_cgroup1_controller_mount(mounts, "memory", mem_rel, mem_mount, mem_root)) {
                    return fail(errc::not_found);
                }
                const auto mem_ancestors = virtualization_common::get_cgroup_ancestor_dirs(
                    mem_mount, mem_root, mem_rel);
                auto mem_limit_res = read_effective_cgroup_limit(
                    mem_ancestors, "memory.limit_in_bytes");
                if (!mem_limit_res) {
                    return fail(mem_limit_res.error());
                }
                record.limits.memory_max_bytes = *mem_limit_res;
            }
        }

        // CPU (if not already set by v2)
        if (!record.limits.cpu_quota_us.has_value()) {
            auto [has_cpu, cpu_rel] = get_v1_controller_info("cpu");
            if (!has_cpu) {
                std::tie(has_cpu, cpu_rel) = get_v1_controller_info("cpuacct");
            }
            if (has_cpu) {
                std::string cpu_mount;
                std::string cpu_root;
                if (!find_cgroup1_controller_mount(mounts, "cpu", cpu_rel, cpu_mount, cpu_root) &&
                    !find_cgroup1_controller_mount(mounts, "cpu,cpuacct", cpu_rel, cpu_mount, cpu_root)) {
                    return fail(errc::not_found);
                }
                const auto cpu_ancestors = virtualization_common::get_cgroup_ancestor_dirs(
                    cpu_mount, cpu_root, cpu_rel);
                auto cpu_res = read_effective_cgroup1_cpu(
                    cpu_ancestors,
                    record.limits.cpu_quota_us, record.limits.cpu_period_us);
                if (!cpu_res) {
                    return fail(cpu_res.error());
                }
            }
        }

        // PIDs (if not already set by v2)
        if (!record.limits.pids_max.has_value()) {
            const auto [has_pids, pids_rel] = get_v1_controller_info("pids");
            if (has_pids) {
                std::string pids_mount;
                std::string pids_root;
                if (!find_cgroup1_controller_mount(mounts, "pids", pids_rel, pids_mount, pids_root)) {
                    return fail(errc::not_found);
                }
                const auto pids_ancestors = virtualization_common::get_cgroup_ancestor_dirs(
                    pids_mount, pids_root, pids_rel);
                auto pids_res = read_effective_cgroup_limit(
                    pids_ancestors, "pids.max");
                if (!pids_res) {
                    return fail(pids_res.error());
                }
                record.limits.pids_max = *pids_res;
            }
        }
    }

    return record;
}

/// Enumerates all active namespace memberships and isolation states for the current process.
inline result<std::vector<virtualization_common::namespace_record>> detect_namespaces() {
    DIR* dir = ::opendir("/proc/self/ns");
    if (dir == nullptr) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    struct dir_closer {
        DIR* handle;
        ~dir_closer() { if (handle != nullptr) { ::closedir(handle); } }
    } closer{dir};

    std::vector<virtualization_common::namespace_record> result_list;
    struct dirent* entry = nullptr;

    std::optional<bool> nested_pid_ns;
    const result<std::string> status_content =
        linux_platform::read_text_file("/proc/self/status", 4096U);
    if (!status_content) {
        if (status_content.error() != std::errc::no_such_file_or_directory) {
            return fail(status_content.error());
        }
    } else {
        const std::string_view content_view = *status_content;
        const std::size_t nspid_pos = content_view.find("NSpid:");
        if (nspid_pos != std::string_view::npos) {
            const std::size_t end_line = content_view.find('\n', nspid_pos);
            const std::string_view line = content_view.substr(
                nspid_pos, end_line == std::string_view::npos
                               ? content_view.size() - nspid_pos
                               : end_line - nspid_pos);
            const auto tokens = virtualization_common::split_whitespace(line);
            nested_pid_ns = (tokens.size() > 2U);
        }
    }

    std::optional<bool> nested_user_ns;
    const result<std::string> uid_map =
        linux_platform::read_text_file("/proc/self/uid_map", 512U);
    if (!uid_map) {
        if (uid_map.error() != std::errc::no_such_file_or_directory) {
            return fail(uid_map.error());
        }
    } else {
        const std::string_view trimmed =
            virtualization_common::trim_signature(*uid_map);
        if (!trimmed.empty()) {
            if (!virtualization_common::is_full_identity_uid_map(trimmed)) {
                nested_user_ns = true;
            }
        }
    }

    errno = 0;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        const std::string name(entry->d_name);
        const std::string path = "/proc/self/ns/" + name;

        struct stat st{};
        if (::stat(path.c_str(), &st) != 0) {
            if (errno == ENOENT) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }

        virtualization_common::namespace_record rec;
        rec.name = name;
        rec.type = virtualization_common::classify_namespace_name(name);
        rec.inode = static_cast<std::uint64_t>(st.st_ino);
        rec.device_id = static_cast<std::uint64_t>(st.st_dev);

        // Evaluate isolation strictly per namespace type
        if (rec.type == virtualization_common::namespace_category::pid ||
            rec.type == virtualization_common::namespace_category::pid_for_children) {
            if (nested_pid_ns.has_value() && *nested_pid_ns) {
                rec.is_isolated = true;
            }
        } else if (rec.type == virtualization_common::namespace_category::user) {
            if (nested_user_ns.has_value()) {
                rec.is_isolated = *nested_user_ns;
            } else {
                rec.is_isolated = std::nullopt;
            }
        }

        if (!rec.is_isolated.has_value()) {
            const std::string init_path = "/proc/1/ns/" + name;
            struct stat st_init{};
            if (::stat(init_path.c_str(), &st_init) == 0) {
                if (st.st_ino != st_init.st_ino) {
                    rec.is_isolated = true;
                } else {
                    if (nested_pid_ns.has_value() && !*nested_pid_ns) {
                        // Confirmed in host root PID namespace, so /proc/1 is host init
                        rec.is_isolated = false;
                    } else {
                        // Nested PID namespace (local container init) or unknown PID status -> cannot prove host equality
                        rec.is_isolated = std::nullopt;
                    }
                }
            } else {
                const int err = errno;
                if (err == ENOENT || err == EACCES || err == EPERM) {
                    rec.is_isolated = std::nullopt;
                } else {
                    return fail(std::error_code(err, std::generic_category()));
                }
            }
        }

        result_list.push_back(std::move(rec));
        errno = 0;
    }

    if (errno != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    std::sort(result_list.begin(), result_list.end(),
              [](const virtualization_common::namespace_record& a,
                 const virtualization_common::namespace_record& b) {
                  return a.name < b.name;
              });

    return result_list;
}

/// Detects whether the current process runs in an isolated namespace.
inline result<bool> detect_is_namespace_isolated() {
    const auto ns_list = detect_namespaces();
    if (!ns_list) {
        return fail(ns_list.error());
    }

    bool has_isolated = false;
    bool has_unknown = false;
    for (const auto& ns : *ns_list) {
        if (ns.is_isolated.has_value()) {
            if (*ns.is_isolated) {
                has_isolated = true;
            }
        } else {
            has_unknown = true;
        }
    }

    if (has_isolated) {
        return true;
    }

    if (!has_unknown) {
        return false;
    }

    // Permission prevented determining isolation state of all namespaces
    return fail(errc::permission_denied);
}

inline result<bool> is_hypervisor_present() {
    const result<hypervisor_info> info = detect_hypervisor();
    if (!info) { return fail(info.error()); }
    return info->present;
}

inline result<virtualization_common::hypervisor_type> hypervisor() {
    const result<hypervisor_info> info = detect_hypervisor();
    if (!info) { return fail(info.error()); }
    return info->vendor;
}

inline result<std::string> hypervisor_name() {
    const result<hypervisor_info> info = detect_hypervisor();
    if (!info) { return fail(info.error()); }
    if (!info->present || info->name.empty()) {
        return fail(errc::not_found);
    }
    return info->name;
}

inline result<bool> is_container() {
    const result<container_info> info = detect_container();
    if (!info) { return fail(info.error()); }
    return info->present;
}

inline result<virtualization_common::container_type> container() {
    const result<container_info> info = detect_container();
    if (!info) { return fail(info.error()); }
    return info->runtime;
}

inline result<std::string> container_name() {
    const result<container_info> info = detect_container();
    if (!info) { return fail(info.error()); }
    if (!info->present || info->name.empty()) {
        return fail(errc::not_found);
    }
    return info->name;
}

inline result<bool> is_wsl() {
    const result<wsl_info> info = detect_wsl();
    if (!info) { return fail(info.error()); }
    return info->is_wsl;
}

inline result<std::uint32_t> wsl_version() {
    const result<wsl_info> info = detect_wsl();
    if (!info) { return fail(info.error()); }
    if (!info->is_wsl) { return fail(errc::not_found); }
    return info->version;
}

inline result<bool> is_sandboxed() {
    const result<sandbox_info> info = detect_sandbox();
    if (!info) { return fail(info.error()); }
    return info->is_sandboxed;
}

inline result<virtualization_common::sandbox_type> sandbox() {
    const result<sandbox_info> info = detect_sandbox();
    if (!info) { return fail(info.error()); }
    return info->type;
}

inline result<virtualization_common::cgroup_version_type> cgroup_hierarchy_version() {
    return detect_cgroup_version();
}

inline result<virtualization_common::cgroup_record> current_cgroup() {
    return detect_current_cgroup();
}

inline result<std::vector<virtualization_common::namespace_record>> namespaces() {
    return detect_namespaces();
}

inline result<bool> is_namespace_isolated() {
    return detect_is_namespace_isolated();
}

} // namespace virtualization_backend
} // namespace detail
} // namespace syscape

#endif
