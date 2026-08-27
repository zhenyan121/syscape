#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_COMMON_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_COMMON_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace virtualization_common {

using hypervisor_type = ::syscape::virtualization::hypervisor_vendor;
using container_type = ::syscape::virtualization::container_runtime;
using sandbox_type = ::syscape::virtualization::sandbox_type;
using cgroup_version_type = ::syscape::virtualization::cgroup_version;
using namespace_category = ::syscape::virtualization::namespace_type;
using namespace_record = ::syscape::virtualization::namespace_info;
using cgroup_limits_record = ::syscape::virtualization::cgroup_limits;
using cgroup_record = ::syscape::virtualization::cgroup_info;

/// Converts one ASCII uppercase letter to lowercase without consulting a locale.
inline unsigned char ascii_lower(unsigned char value) noexcept {
    return value >= static_cast<unsigned char>('A') &&
                   value <= static_cast<unsigned char>('Z')
               ? static_cast<unsigned char>(
                     value + static_cast<unsigned char>('a' - 'A'))
               : value;
}

/// Validates one converted identity text string at the public boundary.
inline result<std::string> validate_identity_text(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty()) { return fail(errc::malformed_data); }
    if (!is_valid_utf8(*value)) { return fail(errc::invalid_encoding); }
    return value;
}

/// Checks if string `text` contains `needle` case-insensitively.
inline bool case_insensitive_contains(std::string_view text,
                                      std::string_view needle) noexcept {
    if (needle.empty()) { return true; }
    if (needle.size() > text.size()) { return false; }
    for (std::size_t i = 0; i <= text.size() - needle.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const auto c1 = static_cast<unsigned char>(text[i + j]);
            const auto c2 = static_cast<unsigned char>(needle[j]);
            if (ascii_lower(c1) != ascii_lower(c2)) {
                match = false;
                break;
            }
        }
        if (match) { return true; }
    }
    return false;
}

/// Checks if two strings are equal case-insensitively.
inline bool case_insensitive_equal(std::string_view a,
                                    std::string_view b) noexcept {
    if (a.size() != b.size()) { return false; }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto c1 = static_cast<unsigned char>(a[i]);
        const auto c2 = static_cast<unsigned char>(b[i]);
        if (ascii_lower(c1) != ascii_lower(c2)) { return false; }
    }
    return true;
}

/// Trims trailing null bytes and whitespace from a signature string.
inline std::string_view trim_signature(std::string_view input) noexcept {
    while (!input.empty() &&
           (input.back() == '\0' || input.back() == ' ' || input.back() == '\t' ||
            input.back() == '\r' || input.back() == '\n')) {
        input.remove_suffix(1U);
    }
    while (!input.empty() &&
           (input.front() == '\0' || input.front() == ' ' || input.front() == '\t' ||
            input.front() == '\r' || input.front() == '\n')) {
        input.remove_prefix(1U);
    }
    return input;
}

/// Decodes the 12-byte CPUID leaf 0x40000000 hypervisor signature.
inline std::string decode_cpuid_signature(std::uint32_t ebx,
                                          std::uint32_t ecx,
                                          std::uint32_t edx) {
    char buffer[12];
    buffer[0] = static_cast<char>(ebx & 0xFFU);
    buffer[1] = static_cast<char>((ebx >> 8U) & 0xFFU);
    buffer[2] = static_cast<char>((ebx >> 16U) & 0xFFU);
    buffer[3] = static_cast<char>((ebx >> 24U) & 0xFFU);
    buffer[4] = static_cast<char>(ecx & 0xFFU);
    buffer[5] = static_cast<char>((ecx >> 8U) & 0xFFU);
    buffer[6] = static_cast<char>((ecx >> 16U) & 0xFFU);
    buffer[7] = static_cast<char>((ecx >> 24U) & 0xFFU);
    buffer[8] = static_cast<char>(edx & 0xFFU);
    buffer[9] = static_cast<char>((edx >> 8U) & 0xFFU);
    buffer[10] = static_cast<char>((edx >> 16U) & 0xFFU);
    buffer[11] = static_cast<char>((edx >> 24U) & 0xFFU);
    const std::string_view trimmed =
        trim_signature(std::string_view(buffer, 12U));
    return std::string(trimmed);
}

/// Maps one CPUID hypervisor signature string onto the hypervisor_vendor vocabulary.
inline hypervisor_type classify_cpuid_signature(std::string_view sig) noexcept {
    const std::string_view trimmed = trim_signature(sig);
    if (trimmed == "KVMKVMKVM") { return hypervisor_type::kvm; }
    if (trimmed == "TCGTCGTCGTCG") { return hypervisor_type::qemu; }
    if (trimmed == "VMwareVMware") { return hypervisor_type::vmware; }
    if (trimmed == "Microsoft Hv") { return hypervisor_type::hyper_v; }
    if (trimmed == "XenVMMXenVMM") { return hypervisor_type::xen; }
    if (trimmed == "bhyve bhyve") { return hypervisor_type::bhyve; }
    if (trimmed == "prl hyperv") {
        return hypervisor_type::parallels;
    }
    if (trimmed == "VBoxVBoxVBox") { return hypervisor_type::virtualbox; }
    if (trimmed == "ACRNACRNACRN") { return hypervisor_type::acrn; }
    if (trimmed == "QNXQVMBSYS") { return hypervisor_type::qnx_hypervisor; }
    if (trimmed == "Apple VMM") { return hypervisor_type::apple_hypervisor; }
    if (!trimmed.empty()) { return hypervisor_type::other; }
    return hypervisor_type::unknown;
}

/// Classifies hypervisor vendor from firmware / DMI strings.
inline hypervisor_type classify_dmi_strings(std::string_view vendor,
                                            std::string_view product,
                                            std::string_view bios_vendor) noexcept {
    if (case_insensitive_contains(vendor, "QEMU") ||
        case_insensitive_contains(product, "QEMU") ||
        case_insensitive_contains(bios_vendor, "QEMU") ||
        case_insensitive_contains(product, "Bochs") ||
        case_insensitive_contains(bios_vendor, "Bochs")) {
        return hypervisor_type::qemu;
    }
    if (case_insensitive_contains(vendor, "VirtualBox") ||
        case_insensitive_contains(product, "VirtualBox") ||
        case_insensitive_contains(bios_vendor, "VirtualBox") ||
        case_insensitive_contains(vendor, "innotek")) {
        return hypervisor_type::virtualbox;
    }
    if (case_insensitive_contains(vendor, "VMware") ||
        case_insensitive_contains(product, "VMware") ||
        case_insensitive_contains(bios_vendor, "VMware")) {
        return hypervisor_type::vmware;
    }
    if (case_insensitive_contains(vendor, "KVM") ||
        case_insensitive_contains(product, "KVM") ||
        case_insensitive_contains(bios_vendor, "KVM")) {
        return hypervisor_type::kvm;
    }
    if (case_insensitive_contains(vendor, "Xen") ||
        case_insensitive_contains(product, "Xen") ||
        case_insensitive_contains(bios_vendor, "Xen")) {
        return hypervisor_type::xen;
    }
    if (case_insensitive_contains(vendor, "Microsoft") &&
        case_insensitive_contains(product, "Virtual Machine")) {
        return hypervisor_type::hyper_v;
    }
    if (case_insensitive_contains(vendor, "Parallels") ||
        case_insensitive_contains(product, "Parallels")) {
        return hypervisor_type::parallels;
    }
    if (case_insensitive_contains(vendor, "bhyve") ||
        case_insensitive_contains(product, "bhyve")) {
        return hypervisor_type::bhyve;
    }
    if (case_insensitive_contains(vendor, "Apple") &&
        (case_insensitive_contains(product, "Virtual") ||
         case_insensitive_contains(product, "Apple Virtualization"))) {
        return hypervisor_type::apple_hypervisor;
    }
    if (case_insensitive_contains(vendor, "Amazon EC2") ||
        case_insensitive_contains(vendor, "Google") ||
        case_insensitive_contains(product, "Google Compute Engine")) {
        // Major cloud hypervisors run KVM-based virtualization.
        return hypervisor_type::kvm;
    }
    return hypervisor_type::unknown;
}

/// Maps a container runtime token string onto the container_runtime enum.
inline container_type classify_container_name(std::string_view name) noexcept {
    const std::string_view trimmed = trim_signature(name);
    if (case_insensitive_equal(trimmed, "docker")) {
        return container_type::docker;
    }
    if (case_insensitive_equal(trimmed, "podman")) {
        return container_type::podman;
    }
    if (case_insensitive_equal(trimmed, "lxc") ||
        case_insensitive_equal(trimmed, "lxc-libvirt")) {
        return container_type::lxc;
    }
    if (case_insensitive_equal(trimmed, "lxd")) {
        return container_type::lxd;
    }
    if (case_insensitive_equal(trimmed, "containerd")) {
        return container_type::containerd;
    }
    if (case_insensitive_equal(trimmed, "kubernetes") ||
        case_insensitive_equal(trimmed, "k8s") ||
        case_insensitive_equal(trimmed, "kubepods")) {
        return container_type::kubernetes;
    }
    if (case_insensitive_equal(trimmed, "systemd-nspawn") ||
        case_insensitive_equal(trimmed, "nspawn")) {
        return container_type::systemd_nspawn;
    }
    if (case_insensitive_equal(trimmed, "openvz") ||
        case_insensitive_equal(trimmed, "vz")) {
        return container_type::openvz;
    }
    if (case_insensitive_equal(trimmed, "wsl")) {
        return container_type::wsl;
    }
    if (case_insensitive_equal(trimmed, "appbox")) {
        return container_type::appbox;
    }
    if (!trimmed.empty()) { return container_type::other; }
    return container_type::unknown;
}

/// Matches a cgroup path hierarchy against known container prefixes.
inline container_type classify_cgroup_line(std::string_view line) noexcept {
    if (case_insensitive_contains(line, "/docker/") ||
        case_insensitive_contains(line, "/docker-") ||
        case_insensitive_contains(line, "docker.service")) {
        return container_type::docker;
    }
    if (case_insensitive_contains(line, "/libpod-") ||
        case_insensitive_contains(line, "/podman/")) {
        return container_type::podman;
    }
    if (case_insensitive_contains(line, "/kubepods/") ||
        case_insensitive_contains(line, "/kubepods.slice/")) {
        return container_type::kubernetes;
    }
    if (case_insensitive_contains(line, "/lxc/") ||
        case_insensitive_contains(line, "/lxc.payload.")) {
        return container_type::lxc;
    }
    if (case_insensitive_contains(line, "/lxd/")) {
        return container_type::lxd;
    }
    if (case_insensitive_contains(line, "/containerd/") ||
        case_insensitive_contains(line, "containerd.service")) {
        return container_type::containerd;
    }
    return container_type::none;
}

/// Maps a Linux namespace name string onto the namespace_type category.
inline namespace_category classify_namespace_name(std::string_view name) noexcept {
    if (name == "cgroup") { return namespace_category::cgroup; }
    if (name == "ipc") { return namespace_category::ipc; }
    if (name == "mnt") { return namespace_category::mount; }
    if (name == "net") { return namespace_category::net; }
    if (name == "pid") { return namespace_category::pid; }
    if (name == "pid_for_children") { return namespace_category::pid_for_children; }
    if (name == "time") { return namespace_category::time; }
    if (name == "time_for_children") { return namespace_category::time_for_children; }
    if (name == "user") { return namespace_category::user; }
    if (name == "uts") { return namespace_category::uts; }
    return namespace_category::unknown;
}

/// Parses a Linux namespace symlink target (e.g. "net:[4026531833]").
inline bool parse_namespace_link(std::string_view link,
                                 std::string_view& type_name,
                                 std::uint64_t& inode) noexcept {
    const std::size_t open_pos = link.find(":[");
    if (open_pos == std::string_view::npos || open_pos == 0U) {
        return false;
    }
    const std::size_t num_start = open_pos + 2U;
    const std::size_t close_pos = link.find(']', num_start);
    if (close_pos == std::string_view::npos || close_pos == num_start || close_pos + 1U != link.size()) {
        return false;
    }
    type_name = link.substr(0U, open_pos);
    const std::string_view num_str = link.substr(num_start, close_pos - num_start);
    std::uint64_t val = 0U;
    for (char c : num_str) {
        if (c < '0' || c > '9') {
            return false;
        }
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (val > (UINT64_MAX - digit) / 10U) {
            return false;
        }
        val = val * 10U + digit;
    }
    inode = val;
    return true;
}

/// Parses a Linux namespace symlink target into an allocated std::string.
inline bool parse_namespace_link(std::string_view link,
                                 std::string& type_name,
                                 std::uint64_t& inode) {
    std::string_view view_name;
    if (!parse_namespace_link(link, view_name, inode)) {
        return false;
    }
    type_name = std::string(view_name);
    return true;
}

/// Parses a cgroup numeric limit value (e.g. integer, "max", or v1 "-1").
inline result<std::optional<std::uint64_t>> parse_cgroup_limit_value(
    std::string_view text) noexcept {
    const std::string_view trimmed = trim_signature(text);
    if (trimmed.empty()) {
        return fail(errc::malformed_data);
    }
    if (trimmed == "max") {
        return std::optional<std::uint64_t>(std::nullopt);
    }
    if (trimmed == "-1") {
        // Explicit cgroup v1 unlimited sentinel
        return std::optional<std::uint64_t>(std::nullopt);
    }
    if (trimmed.front() == '-') {
        // Other negative values (e.g. -2, -garbage) are invalid
        return fail(errc::malformed_data);
    }
    std::uint64_t val = 0U;
    for (char c : trimmed) {
        if (c < '0' || c > '9') {
            return fail(errc::malformed_data);
        }
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (val > (UINT64_MAX - digit) / 10U) {
            return fail(errc::malformed_data);
        }
        val = val * 10U + digit;
    }
    // Sentinel check: Linux cgroup v1 PAGE_COUNTER_MAX or near INT64_MAX
    if (val >= 0x7FFFFFFFFFFFF000ULL) {
        return std::optional<std::uint64_t>(std::nullopt);
    }
    return std::optional<std::uint64_t>(val);
}

/// Parses a cgroup v1 CPU quota, for which only -1 means unlimited.
inline result<std::optional<std::uint64_t>> parse_cgroup1_cpu_quota_value(
    std::string_view text) noexcept {
    const std::string_view trimmed = trim_signature(text);
    if (trimmed == "-1") {
        return std::optional<std::uint64_t>(std::nullopt);
    }
    if (trimmed.empty() || trimmed.front() == '-') {
        return fail(errc::malformed_data);
    }

    std::uint64_t value = 0U;
    for (char c : trimmed) {
        if (c < '0' || c > '9') {
            return fail(errc::malformed_data);
        }
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (value > (UINT64_MAX - digit) / 10U) {
            return fail(errc::malformed_data);
        }
        value = value * 10U + digit;
    }
    if (value == 0U || value > 9223372036854775807ULL) {
        return fail(errc::malformed_data);
    }
    return std::optional<std::uint64_t>(value);
}

/// Compares two positive rational values exactly without overflowing.
inline bool positive_ratio_less(std::uint64_t lhs_numerator,
                                std::uint64_t lhs_denominator,
                                std::uint64_t rhs_numerator,
                                std::uint64_t rhs_denominator) noexcept {
    bool inverted = false;
    for (;;) {
        const std::uint64_t lhs_quotient = lhs_numerator / lhs_denominator;
        const std::uint64_t rhs_quotient = rhs_numerator / rhs_denominator;
        if (lhs_quotient != rhs_quotient) {
            return inverted ? lhs_quotient > rhs_quotient
                            : lhs_quotient < rhs_quotient;
        }

        const std::uint64_t lhs_remainder = lhs_numerator % lhs_denominator;
        const std::uint64_t rhs_remainder = rhs_numerator % rhs_denominator;
        if (lhs_remainder == 0U || rhs_remainder == 0U) {
            if (lhs_remainder == 0U && rhs_remainder == 0U) {
                return false;
            }
            const bool less_before_inversion = lhs_remainder == 0U;
            return inverted ? !less_before_inversion : less_before_inversion;
        }

        lhs_numerator = lhs_denominator;
        lhs_denominator = lhs_remainder;
        rhs_numerator = rhs_denominator;
        rhs_denominator = rhs_remainder;
        inverted = !inverted;
    }
}

/// Unescapes octal sequences (e.g. \040 for space) in /proc/self/mountinfo paths.
inline std::string unescape_mountinfo_path(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    std::size_t i = 0U;
    while (i < text.size()) {
        if (text[i] == '\\' && i + 3U < text.size() &&
            text[i + 1U] >= '0' && text[i + 1U] <= '7' &&
            text[i + 2U] >= '0' && text[i + 2U] <= '7' &&
            text[i + 3U] >= '0' && text[i + 3U] <= '7') {
            const auto oct_val = static_cast<char>(
                ((text[i + 1U] - '0') << 6) |
                ((text[i + 2U] - '0') << 3) |
                (text[i + 3U] - '0'));
            result.push_back(oct_val);
            i += 4U;
        } else {
            result.push_back(text[i]);
            ++i;
        }
    }
    return result;
}

/// Splits text by whitespace (spaces, tabs, newlines).
inline std::vector<std::string_view> split_whitespace(std::string_view text) {
    std::vector<std::string_view> tokens;
    std::size_t start = 0U;
    while (start < text.size()) {
        while (start < text.size() && (text[start] == ' ' || text[start] == '\t' ||
                                       text[start] == '\r' || text[start] == '\n')) {
            ++start;
        }
        if (start >= text.size()) { break; }
        std::size_t end = start;
        while (end < text.size() && text[end] != ' ' && text[end] != '\t' &&
               text[end] != '\r' && text[end] != '\n') {
            ++end;
        }
        tokens.push_back(text.substr(start, end - start));
        start = end;
    }
    return tokens;
}

/// Checks whether a Linux UID map is the initial namespace's full identity map.
inline bool is_full_identity_uid_map(std::string_view text) {
    const auto fields = split_whitespace(text);
    return fields.size() == 3U && fields[0] == "0" && fields[1] == "0" &&
           fields[2] == "4294967295";
}

/// Splits text strictly by commas and trims whitespace.
inline std::vector<std::string> split_commas(std::string_view text) {
    std::vector<std::string> tokens;
    std::size_t start = 0U;
    while (start < text.size()) {
        const std::size_t comma_pos = text.find(',', start);
        const std::string_view token = text.substr(
            start, comma_pos == std::string_view::npos ? text.size() - start : comma_pos - start);
        const std::string_view trimmed = trim_signature(token);
        if (!trimmed.empty()) {
            tokens.emplace_back(trimmed);
        }
        if (comma_pos == std::string_view::npos) {
            break;
        }
        start = comma_pos + 1U;
    }
    return tokens;
}

/// Parses cgroup v2 cpu.max contents (strictly "<quota> <period>").
inline result<void> parse_cgroup_cpu_max(
    std::string_view text,
    std::optional<std::uint64_t>& quota_us,
    std::optional<std::uint64_t>& period_us) {
    const auto tokens = split_whitespace(text);
    if (tokens.size() != 2U) {
        return fail(errc::malformed_data);
    }

    const std::string_view quota_tok = tokens[0];
    const std::string_view period_tok = tokens[1];

    if (quota_tok == "max") {
        quota_us = std::nullopt;
    } else {
        const auto q_res = parse_cgroup_limit_value(quota_tok);
        if (!q_res) { return fail(q_res.error()); }
        if (!q_res->has_value() || **q_res == 0U) {
            return fail(errc::malformed_data);
        }
        quota_us = *q_res;
    }

    // Period must be a positive integer > 0 (cannot be "max", "-1", or 0)
    if (period_tok.empty() || period_tok == "max" || period_tok.front() == '-') {
        return fail(errc::malformed_data);
    }
    std::uint64_t period_val = 0U;
    for (char c : period_tok) {
        if (c < '0' || c > '9') {
            return fail(errc::malformed_data);
        }
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (period_val > (UINT64_MAX - digit) / 10U) {
            return fail(errc::malformed_data);
        }
        period_val = period_val * 10U + digit;
    }
    if (period_val == 0U) {
        return fail(errc::malformed_data);
    }
    period_us = period_val;
    return {};
}

/// Computes the list of full filesystem directory paths from the cgroup node up to the mount point.
inline std::vector<std::string> get_cgroup_ancestor_dirs(
    const std::string& mount_point,
    const std::string& mount_root,
    const std::string& proc_cgroup_path) {
    std::string rel_path;
    if (mount_root == "/" || mount_root.empty()) {
        rel_path = proc_cgroup_path;
    } else if (proc_cgroup_path == mount_root) {
        rel_path = "";
    } else if (proc_cgroup_path.size() > mount_root.size() &&
               proc_cgroup_path.compare(0, mount_root.size(), mount_root) == 0 &&
               proc_cgroup_path[mount_root.size()] == '/') {
        rel_path = proc_cgroup_path.substr(mount_root.size());
    } else {
        rel_path = "";
    }

    while (rel_path.size() > 1U && rel_path.back() == '/') {
        rel_path.pop_back();
    }
    if (rel_path == "/") {
        rel_path.clear();
    }

    std::vector<std::string> ancestors;
    std::string current = rel_path;
    while (!current.empty()) {
        std::string full = mount_point;
        if (full.empty() || full.back() != '/') {
            full += '/';
        }
        if (current.front() == '/') {
            full += current.substr(1U);
        } else {
            full += current;
        }
        ancestors.push_back(std::move(full));

        const std::size_t last_slash = current.rfind('/');
        if (last_slash == std::string::npos || last_slash == 0U) {
            break;
        }
        current = current.substr(0U, last_slash);
    }
    ancestors.push_back(mount_point);
    return ancestors;
}

/// Splits space- or comma-separated cgroup controllers list.
inline std::vector<std::string> split_cgroup_controllers(std::string_view text) {
    std::vector<std::string> result_list;
    std::size_t start = 0U;
    while (start < text.size()) {
        while (start < text.size() && (text[start] == ' ' || text[start] == '\t' ||
                                       text[start] == '\r' || text[start] == '\n' ||
                                       text[start] == ',')) {
            ++start;
        }
        if (start >= text.size()) { break; }
        std::size_t end = start;
        while (end < text.size() && text[end] != ' ' && text[end] != '\t' &&
               text[end] != '\r' && text[end] != '\n' && text[end] != ',') {
            ++end;
        }
        result_list.emplace_back(text.substr(start, end - start));
        start = end;
    }
    return result_list;
}

/// Checks if mount_root is a prefix of proc_cgroup_path.
inline bool is_mount_root_prefix(std::string_view mount_root, std::string_view proc_cgroup_path) noexcept {
    if (mount_root.empty() || mount_root == "/") {
        return true;
    }
    if (mount_root == proc_cgroup_path) {
        return true;
    }
    if (proc_cgroup_path.size() > mount_root.size() &&
        proc_cgroup_path.compare(0, mount_root.size(), mount_root) == 0 &&
        proc_cgroup_path[mount_root.size()] == '/') {
        return true;
    }
    return false;
}

/// Description of a cgroup v1 hierarchy entry.
struct cgroup_v1_hierarchy {
    std::vector<std::string> controllers;
    std::string path;
};

/// Structure representing the parsed contents of /proc/self/cgroup.
struct parsed_cgroup_info {
    cgroup_version_type version = cgroup_version_type::none;
    std::string v2_path;
    std::vector<cgroup_v1_hierarchy> v1_hierarchies;
};

/// Parses /proc/self/cgroup file to extract v2 path and all v1 hierarchy paths with validation.
inline result<parsed_cgroup_info> parse_cgroup_proc_file_detailed(std::string_view cgroup_content) {
    parsed_cgroup_info parsed;
    bool has_v2 = false;
    bool has_v1 = false;

    std::size_t offset = 0U;
    while (offset < cgroup_content.size()) {
        const std::size_t end = cgroup_content.find('\n', offset);
        std::string_view line = cgroup_content.substr(
            offset, end == std::string_view::npos
                        ? cgroup_content.size() - offset
                        : end - offset);
        offset = end == std::string_view::npos ? cgroup_content.size() : end + 1U;

        // Strip only line terminators (\r, \n), preserving valid trailing spaces in cgroup names
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.remove_suffix(1U);
        }
        if (line.empty()) { continue; }

        const std::size_t first_colon = line.find(':');
        if (first_colon == std::string_view::npos) {
            return fail(errc::malformed_data);
        }
        const std::string_view id_str = line.substr(0U, first_colon);
        if (id_str.empty()) {
            return fail(errc::malformed_data);
        }
        std::uint64_t hier_id = 0U;
        for (char c : id_str) {
            if (c < '0' || c > '9') {
                return fail(errc::malformed_data);
            }
            const auto digit = static_cast<std::uint64_t>(c - '0');
            if (hier_id > (UINT64_MAX - digit) / 10U) {
                return fail(errc::malformed_data);
            }
            hier_id = hier_id * 10U + digit;
        }

        const std::size_t second_colon = line.find(':', first_colon + 1U);
        if (second_colon == std::string_view::npos) {
            return fail(errc::malformed_data);
        }
        const std::string_view ctrl_names_str =
            line.substr(first_colon + 1U, second_colon - first_colon - 1U);
        const std::string_view ctrl_path_str = line.substr(second_colon + 1U);
        if (ctrl_path_str.empty() || ctrl_path_str.front() != '/') {
            return fail(errc::malformed_data);
        }

        if (hier_id == 0U) {
            if (!ctrl_names_str.empty()) {
                return fail(errc::malformed_data);
            }
            has_v2 = true;
            if (parsed.v2_path.empty()) {
                parsed.v2_path = std::string(ctrl_path_str);
            }
        } else {
            if (ctrl_names_str.empty()) {
                return fail(errc::malformed_data);
            }
            has_v1 = true;
            cgroup_v1_hierarchy hier;
            hier.controllers = split_commas(ctrl_names_str);
            if (hier.controllers.empty()) {
                return fail(errc::malformed_data);
            }
            hier.path = std::string(ctrl_path_str);
            parsed.v1_hierarchies.push_back(std::move(hier));
        }
    }

    if (has_v2 && has_v1) {
        parsed.version = cgroup_version_type::hybrid;
    } else if (has_v2) {
        parsed.version = cgroup_version_type::v2;
    } else if (has_v1) {
        parsed.version = cgroup_version_type::v1;
    }
    return parsed;
}

/// Compatibility parser for /proc/self/cgroup file.
inline void parse_cgroup_proc_file(std::string_view cgroup_content,
                                   cgroup_version_type& version,
                                   std::string& primary_path) {
    const auto detailed = parse_cgroup_proc_file_detailed(cgroup_content);
    if (!detailed) {
        version = cgroup_version_type::none;
        primary_path.clear();
        return;
    }
    version = detailed->version;
    if (!detailed->v2_path.empty()) {
        primary_path = detailed->v2_path;
    } else if (!detailed->v1_hierarchies.empty()) {
        primary_path = detailed->v1_hierarchies.front().path;
    } else {
        primary_path.clear();
    }
}

} // namespace virtualization_common
} // namespace detail
} // namespace syscape

#endif
