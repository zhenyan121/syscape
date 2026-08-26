#ifndef SYSCAPE_DETAIL_SOFTWARE_LINUX_HPP
#define SYSCAPE_DETAIL_SOFTWARE_LINUX_HPP

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <pwd.h>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <vector>

#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/software/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace software_backend {
namespace linux_impl {

inline std::string_view trim_whitespace_view(std::string_view sv) noexcept {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return sv;
}

inline bool parse_uint64(std::string_view sv, std::uint64_t& out) noexcept {
    sv = trim_whitespace_view(sv);
    if (sv.empty()) {
        return false;
    }
    std::uint64_t val = 0;
    for (const char c : sv) {
        if (c < '0' || c > '9') {
            return false;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
        if (val > (UINT64_MAX - digit) / 10) {
            return false;
        }
        val = val * 10 + digit;
    }
    out = val;
    return true;
}

inline bool parse_uint32(std::string_view sv, std::uint32_t& out) noexcept {
    std::uint64_t val64 = 0;
    if (!parse_uint64(sv, val64) || val64 > UINT32_MAX) {
        return false;
    }
    out = static_cast<std::uint32_t>(val64);
    return true;
}

// ----------------------------------------------------------------------------
// Kernel Driver / Module Parsing (/proc/modules)
// ----------------------------------------------------------------------------

inline bool parse_proc_modules_line(
    std::string_view line,
    software_common::driver_record& out) {
    line = trim_whitespace_view(line);
    if (line.empty() || line.front() == '#') {
        return false;
    }

    // Tokens: <name> <size> <use_count> <used_by> <state> <address>
    std::vector<std::string_view> tokens;
    std::size_t start = 0;
    while (start < line.size()) {
        while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
            ++start;
        }
        if (start >= line.size()) {
            break;
        }
        std::size_t end = start;
        while (end < line.size() && !std::isspace(static_cast<unsigned char>(line[end]))) {
            ++end;
        }
        tokens.push_back(line.substr(start, end - start));
        start = end;
    }

    if (tokens.size() < 6 || tokens[0].empty()) {
        return false;
    }

    out = software_common::driver_record {};
    out.name = std::string(tokens[0]);

    std::uint64_t size_val = 0;
    if (!parse_uint64(tokens[1], size_val)) {
        return false;
    }
    out.size_bytes = size_val;

    std::uint32_t count_val = 0;
    if (!parse_uint32(tokens[2], count_val)) {
        return false;
    }
    out.use_count = count_val;

    const std::string_view state_str = tokens[4];
    if (state_str == "Live") {
        out.state = software_common::driver_state::running;
    } else if (state_str == "Loading") {
        out.state = software_common::driver_state::unknown;
    } else if (state_str == "Unloading") {
        out.state = software_common::driver_state::unloading;
    } else {
        out.state = software_common::driver_state::unknown;
    }

    return true;
}

inline bool parse_proc_modules(
    std::string_view content,
    std::vector<software_common::driver_record>& out) {
    std::size_t pos = 0;
    while (pos < content.size()) {
        const std::size_t next_line = content.find('\n', pos);
        const std::size_t len = (next_line == std::string_view::npos) ? content.size() - pos : next_line - pos;
        const std::string_view line = content.substr(pos, len);
        pos = (next_line == std::string_view::npos) ? content.size() : next_line + 1;

        const std::string_view trimmed = trim_whitespace_view(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        software_common::driver_record rec;
        if (!parse_proc_modules_line(trimmed, rec)) {
            return false;
        }
        out.push_back(std::move(rec));
    }
    return true;
}

// ----------------------------------------------------------------------------
// Systemd Service Unit File Parsing
// ----------------------------------------------------------------------------

inline bool parse_systemd_service_file(
    std::string_view content,
    std::string_view unit_name,
    software_common::service_record& out) {
    out = software_common::service_record {};
    out.name = std::string(unit_name);
    out.startup_type = software_common::service_startup::unknown;
    out.state = software_common::service_state::unknown;

    std::string current_section;
    bool has_install_section = false;
    std::size_t pos = 0;

    while (pos < content.size()) {
        const std::size_t next_line = content.find('\n', pos);
        const std::size_t len = (next_line == std::string_view::npos) ? content.size() - pos : next_line - pos;
        std::string_view line = trim_whitespace_view(content.substr(pos, len));
        pos = (next_line == std::string_view::npos) ? content.size() : next_line + 1;

        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            current_section = std::string(line.substr(1, line.size() - 2));
            if (current_section == "Install") {
                has_install_section = true;
            }
            continue;
        }

        const std::size_t eq_pos = line.find('=');
        if (eq_pos == std::string_view::npos) {
            continue;
        }

        const std::string_view key = trim_whitespace_view(line.substr(0, eq_pos));
        const std::string_view val = trim_whitespace_view(line.substr(eq_pos + 1));

        if (current_section == "Unit") {
            if (key == "Description" && !out.description.has_value()) {
                out.description = std::string(val);
                out.display_name = std::string(val);
            }
        } else if (current_section == "Service") {
            if (key == "ExecStart" && !out.executable_path.has_value()) {
                out.executable_path = std::string(val);
            }
        }
    }

    if (has_install_section) {
        out.startup_type = software_common::service_startup::manual;
    }
    return true;
}

inline void scan_systemd_wants_dir(
    const char* base_dir,
    std::unordered_set<std::string>& enabled_services,
    std::error_code& last_error) {
    linux_platform::directory_handle dir(base_dir);
    if (!dir.valid()) {
        if (dir.error() != ENOENT) {
            last_error = std::error_code(dir.error(), std::generic_category());
        }
        return;
    }

    for (;;) {
        errno = 0;
        struct dirent* entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                last_error = std::error_code(errno, std::generic_category());
            }
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }
        const std::string name(entry->d_name);
        const std::string wants_suffix = ".wants";
        const std::string req_suffix = ".requires";
        const bool is_wants = (name.size() > wants_suffix.size() &&
                               name.compare(name.size() - wants_suffix.size(), wants_suffix.size(), wants_suffix) == 0);
        const bool is_req = (name.size() > req_suffix.size() &&
                             name.compare(name.size() - req_suffix.size(), req_suffix.size(), req_suffix) == 0);

        if (!is_wants && !is_req) {
            continue;
        }

        const std::string sub_path = std::string(base_dir) + "/" + name;
        linux_platform::directory_handle sub_dir(sub_path.c_str());
        if (!sub_dir.valid()) {
            if (sub_dir.error() != ENOENT) {
                last_error = std::error_code(sub_dir.error(), std::generic_category());
            }
            continue;
        }

        for (;;) {
            errno = 0;
            struct dirent* sub_entry = ::readdir(sub_dir.get());
            if (sub_entry == nullptr) {
                if (errno != 0) {
                    last_error = std::error_code(errno, std::generic_category());
                }
                break;
            }
            if (sub_entry->d_name[0] == '.') {
                continue;
            }
            const std::string target_file(sub_entry->d_name);
            const std::string svc_suffix = ".service";
            if (target_file.size() > svc_suffix.size() &&
                target_file.compare(target_file.size() - svc_suffix.size(), svc_suffix.size(), svc_suffix) == 0) {
                enabled_services.insert(target_file);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Package Database Parsing: Pacman (Arch Linux /var/lib/pacman/local/*/desc)
// ----------------------------------------------------------------------------

inline bool parse_pacman_desc(
    std::string_view content,
    software_common::package_record& out) {
    out = software_common::package_record {};
    out.format = software_common::package_format::pacman;

    std::string current_tag;
    std::size_t pos = 0;

    while (pos < content.size()) {
        const std::size_t next_line = content.find('\n', pos);
        const std::size_t len = (next_line == std::string_view::npos) ? content.size() - pos : next_line - pos;
        std::string_view line = trim_whitespace_view(content.substr(pos, len));
        pos = (next_line == std::string_view::npos) ? content.size() : next_line + 1;

        if (line.empty()) {
            continue;
        }

        if (line.front() == '%' && line.back() == '%' && line.size() >= 3) {
            current_tag = std::string(line.substr(1, line.size() - 2));
            continue;
        }

        if (current_tag == "NAME" && out.name.empty()) {
            out.name = std::string(line);
        } else if (current_tag == "VERSION" && !out.version.has_value()) {
            out.version = std::string(line);
        } else if (current_tag == "DESC" && !out.description.has_value()) {
            out.description = std::string(line);
        } else if (current_tag == "ARCH" && !out.architecture.has_value()) {
            out.architecture = std::string(line);
        } else if (current_tag == "PACKAGER" && !out.publisher.has_value()) {
            out.publisher = std::string(line);
        }
    }

    return !out.name.empty();
}

// ----------------------------------------------------------------------------
// Package Database Parsing: dpkg (Debian/Ubuntu /var/lib/dpkg/status)
// ----------------------------------------------------------------------------

inline bool parse_dpkg_status(
    std::string_view content,
    std::vector<software_common::package_record>& out) {
    std::size_t pos = 0;
    software_common::package_record current;
    current.format = software_common::package_format::dpkg;
    bool is_installed = false;

    auto finish_record = [&]() {
        if (!current.name.empty() && is_installed) {
            out.push_back(std::move(current));
        }
        current = software_common::package_record {};
        current.format = software_common::package_format::dpkg;
        is_installed = false;
    };

    while (pos < content.size()) {
        const std::size_t next_line = content.find('\n', pos);
        const std::size_t len = (next_line == std::string_view::npos) ? content.size() - pos : next_line - pos;
        const std::string_view raw_line = content.substr(pos, len);
        std::string_view line = trim_whitespace_view(raw_line);
        pos = (next_line == std::string_view::npos) ? content.size() : next_line + 1;

        if (line.empty()) {
            finish_record();
            continue;
        }

        if (raw_line.front() == ' ' || raw_line.front() == '\t') {
            // Continuation line for multiline fields like Description
            continue;
        }

        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }

        const std::string_view key = trim_whitespace_view(line.substr(0, colon));
        const std::string_view val = trim_whitespace_view(line.substr(colon + 1));

        if (key == "Package") {
            current.name = std::string(val);
        } else if (key == "Version") {
            current.version = std::string(val);
        } else if (key == "Architecture") {
            current.architecture = std::string(val);
        } else if (key == "Maintainer") {
            current.publisher = std::string(val);
        } else if (key == "Description") {
            current.description = std::string(val);
        } else if (key == "Status") {
            is_installed = (val == "install ok installed");
        }
    }

    finish_record();
    return true;
}

// ----------------------------------------------------------------------------
// Package Database Parsing: Alpine (/lib/apk/db/installed)
// ----------------------------------------------------------------------------

inline bool parse_apk_installed(
    std::string_view content,
    std::vector<software_common::package_record>& out) {
    std::size_t pos = 0;
    software_common::package_record current;
    current.format = software_common::package_format::apk;

    auto finish_record = [&]() {
        if (!current.name.empty()) {
            out.push_back(std::move(current));
        }
        current = software_common::package_record {};
        current.format = software_common::package_format::apk;
    };

    while (pos < content.size()) {
        const std::size_t next_line = content.find('\n', pos);
        const std::size_t len = (next_line == std::string_view::npos) ? content.size() - pos : next_line - pos;
        std::string_view line = trim_whitespace_view(content.substr(pos, len));
        pos = (next_line == std::string_view::npos) ? content.size() : next_line + 1;

        if (line.empty()) {
            finish_record();
            continue;
        }

        if (line.size() >= 2 && line[1] == ':') {
            const char tag = line[0];
            const std::string_view val = trim_whitespace_view(line.substr(2));
            if (tag == 'P') {
                current.name = std::string(val);
            } else if (tag == 'V') {
                current.version = std::string(val);
            } else if (tag == 'T') {
                current.description = std::string(val);
            } else if (tag == 'A') {
                current.architecture = std::string(val);
            } else if (tag == 'm') {
                current.publisher = std::string(val);
            }
        }
    }

    finish_record();
    return true;
}

// ----------------------------------------------------------------------------
// Desktop Application Entry Parsing (*.desktop)
// ----------------------------------------------------------------------------

inline bool parse_desktop_entry(
    std::string_view content,
    std::string_view /*desktop_file_path*/,
    software_common::package_record& out) {
    out = software_common::package_record {};
    out.format = software_common::package_format::desktop_entry;

    bool in_desktop_entry = false;
    bool is_application_type = false;
    std::size_t pos = 0;
    std::string generic_name;

    while (pos < content.size()) {
        const std::size_t next_line = content.find('\n', pos);
        const std::size_t len = (next_line == std::string_view::npos) ? content.size() - pos : next_line - pos;
        std::string_view line = trim_whitespace_view(content.substr(pos, len));
        pos = (next_line == std::string_view::npos) ? content.size() : next_line + 1;

        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            in_desktop_entry = (line == "[Desktop Entry]");
            continue;
        }

        if (!in_desktop_entry) {
            continue;
        }

        const std::size_t eq_pos = line.find('=');
        if (eq_pos == std::string_view::npos) {
            continue;
        }

        const std::string_view key = trim_whitespace_view(line.substr(0, eq_pos));
        const std::string_view val = trim_whitespace_view(line.substr(eq_pos + 1));

        if (key == "Type") {
            if (val == "Application") {
                is_application_type = true;
            }
        } else if (key == "Name" && out.name.empty()) {
            out.name = std::string(val);
        } else if (key == "Version" && !out.version.has_value()) {
            out.version = std::string(val);
        } else if (key == "Comment" && !out.description.has_value()) {
            out.description = std::string(val);
        } else if (key == "GenericName" && generic_name.empty()) {
            generic_name = std::string(val);
        }
    }

    if (!is_application_type) {
        return false;
    }

    if (!out.description.has_value() && !generic_name.empty()) {
        out.description = generic_name;
    }

    return !out.name.empty();
}

inline result<std::optional<std::string>> get_user_home_dir() {
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return std::optional<std::string>{std::string(home)};
    }
    const long sc_size = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    std::size_t buf_size = (sc_size > 0) ? static_cast<std::size_t>(sc_size) : 1024U;
    struct passwd pwd;
    struct passwd* result = nullptr;
    std::vector<char> buffer;

    for (int attempt = 0; attempt < 5; ++attempt) {
        buffer.resize(buf_size);
        const int err = ::getpwuid_r(::getuid(), &pwd, buffer.data(), buffer.size(), &result);
        if (err == 0) {
            if (result != nullptr && result->pw_dir != nullptr && result->pw_dir[0] != '\0') {
                return std::optional<std::string>{std::string(result->pw_dir)};
            }
            return std::optional<std::string>{std::nullopt};
        }
        if (err == ERANGE) {
            buf_size *= 2;
            continue;
        }
        return fail(std::error_code(err, std::generic_category()));
    }
    return fail(errc::value_too_large);
}

} // namespace linux_impl

// ----------------------------------------------------------------------------
// Backend Query Implementations
// ----------------------------------------------------------------------------

inline result<std::vector<software_common::driver_record>> loaded_drivers() {
    const result<std::string> content = linux_platform::read_text_file("/proc/modules", 2U * 1024U * 1024U);
    if (!content) {
        if (content.error() == std::errc::no_such_file_or_directory) {
            return fail(errc::not_supported);
        }
        return fail(content.error());
    }

    std::vector<software_common::driver_record> drivers;
    if (!linux_impl::parse_proc_modules(*content, drivers)) {
        return fail(errc::malformed_data);
    }
    std::sort(drivers.begin(), drivers.end(), software_common::compare_drivers);
    return drivers;
}

inline result<software_common::driver_record> find_driver(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    const result<std::vector<software_common::driver_record>> all = loaded_drivers();
    if (!all) {
        return fail(all.error());
    }
    for (const auto& item : *all) {
        if (item.name == name) {
            return item;
        }
    }
    return fail(errc::not_found);
}

inline result<std::vector<software_common::service_record>> services() {
    static const char* const service_dirs[] = {
        "/etc/systemd/system",
        "/run/systemd/system",
        "/usr/lib/systemd/system",
        "/lib/systemd/system"
    };

    std::error_code last_error;
    std::unordered_set<std::string> enabled_services;
    for (const char* dir_path : service_dirs) {
        linux_impl::scan_systemd_wants_dir(dir_path, enabled_services, last_error);
    }

    std::unordered_set<std::string> seen_names;
    std::vector<software_common::service_record> result_services;
    bool any_dir_opened = false;

    for (const char* dir_path : service_dirs) {
        linux_platform::directory_handle dir(dir_path);
        if (!dir.valid()) {
            if (dir.error() != ENOENT) {
                last_error = std::error_code(dir.error(), std::generic_category());
            }
            continue;
        }
        any_dir_opened = true;

        for (;;) {
            errno = 0;
            struct dirent* entry = ::readdir(dir.get());
            if (entry == nullptr) {
                if (errno != 0) {
                    last_error = std::error_code(errno, std::generic_category());
                }
                break;
            }
            if (entry->d_name[0] == '.') {
                continue;
            }
            const std::string filename(entry->d_name);
            const std::string suffix = ".service";
            if (filename.size() <= suffix.size() ||
                filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0) {
                continue;
            }

            if (seen_names.find(filename) != seen_names.end()) {
                continue;
            }
            seen_names.insert(filename);

            const std::string full_path = std::string(dir_path) + "/" + filename;

            char link_target[256] = {0};
            const ssize_t link_len = ::readlink(full_path.c_str(), link_target, sizeof(link_target) - 1);
            const bool is_masked = (link_len > 0 && std::string_view(link_target, static_cast<std::size_t>(link_len)) == "/dev/null");

            if (is_masked) {
                software_common::service_record rec;
                rec.name = filename;
                rec.startup_type = software_common::service_startup::disabled;
                rec.state = software_common::service_state::stopped;
                result_services.push_back(std::move(rec));
                continue;
            }

            const result<std::string> content = linux_platform::read_text_file(full_path.c_str(), 128U * 1024U);
            if (!content) {
                if (content.error() != std::errc::no_such_file_or_directory) {
                    last_error = content.error();
                }
                continue;
            }

            software_common::service_record rec;
            if (linux_impl::parse_systemd_service_file(*content, filename, rec)) {
                if (enabled_services.find(filename) != enabled_services.end()) {
                    rec.startup_type = software_common::service_startup::automatic;
                } else {
                    rec.startup_type = software_common::service_startup::manual;
                }
                result_services.push_back(std::move(rec));
            }
        }
    }

    if (!any_dir_opened) {
        linux_platform::directory_handle init_dir("/etc/init.d");
        if (init_dir.valid()) {
            any_dir_opened = true;
            for (;;) {
                errno = 0;
                struct dirent* entry = ::readdir(init_dir.get());
                if (entry == nullptr) {
                    if (errno != 0) {
                        last_error = std::error_code(errno, std::generic_category());
                    }
                    break;
                }
                if (entry->d_name[0] == '.') {
                    continue;
                }
                const std::string name(entry->d_name);
                if (name == "README" || name == "skeleton") {
                    continue;
                }
                software_common::service_record rec;
                rec.name = name;
                rec.executable_path = std::string("/etc/init.d/") + name;
                rec.state = software_common::service_state::unknown;
                rec.startup_type = software_common::service_startup::unknown;
                result_services.push_back(std::move(rec));
            }
        } else if (init_dir.error() != ENOENT) {
            last_error = std::error_code(init_dir.error(), std::generic_category());
        }
    }

    if (last_error) {
        return fail(last_error);
    }

    if (!any_dir_opened && result_services.empty()) {
        return fail(errc::not_supported);
    }

    std::sort(result_services.begin(), result_services.end(), software_common::compare_services);
    return result_services;
}

inline result<software_common::service_record> find_service(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    const result<std::vector<software_common::service_record>> all = services();
    if (!all) {
        return fail(all.error());
    }

    const std::string exact_name(name);
    const std::string with_suffix = exact_name.find('.') == std::string::npos ? exact_name + ".service" : exact_name;

    for (const auto& item : *all) {
        if (item.name == exact_name || item.name == with_suffix) {
            return item;
        }
    }
    return fail(errc::not_found);
}

inline result<std::vector<software_common::package_record>> installed_packages() {
    std::vector<software_common::package_record> pkgs;
    std::unordered_set<std::string> seen_keys;
    bool any_source_found = false;
    std::error_code last_error;

    auto add_unique_package = [&](software_common::package_record&& rec) {
        const std::string key = software_common::make_package_dedup_key(rec);
        if (seen_keys.find(key) == seen_keys.end()) {
            seen_keys.insert(key);
            pkgs.push_back(std::move(rec));
        }
    };

    // 1. Check Pacman (/var/lib/pacman/local)
    linux_platform::directory_handle pacman_dir("/var/lib/pacman/local");
    if (pacman_dir.valid()) {
        any_source_found = true;
        for (;;) {
            errno = 0;
            struct dirent* entry = ::readdir(pacman_dir.get());
            if (entry == nullptr) {
                if (errno != 0) {
                    last_error = std::error_code(errno, std::generic_category());
                }
                break;
            }
            if (entry->d_name[0] == '.') {
                continue;
            }
            const std::string desc_path = std::string("/var/lib/pacman/local/") + entry->d_name + "/desc";
            const result<std::string> content = linux_platform::read_text_file(desc_path.c_str(), 128U * 1024U);
            if (!content) {
                if (content.error() != std::errc::no_such_file_or_directory) {
                    last_error = content.error();
                }
                continue;
            }
            software_common::package_record rec;
            if (linux_impl::parse_pacman_desc(*content, rec)) {
                add_unique_package(std::move(rec));
            }
        }
    } else if (pacman_dir.error() != ENOENT) {
        last_error = std::error_code(pacman_dir.error(), std::generic_category());
    }

    // 2. Check dpkg (/var/lib/dpkg/status)
    const result<std::string> dpkg_content = linux_platform::read_text_file("/var/lib/dpkg/status", 32U * 1024U * 1024U);
    if (dpkg_content && !dpkg_content->empty()) {
        any_source_found = true;
        std::vector<software_common::package_record> dpkg_pkgs;
        linux_impl::parse_dpkg_status(*dpkg_content, dpkg_pkgs);
        for (auto& item : dpkg_pkgs) {
            add_unique_package(std::move(item));
        }
    } else if (!dpkg_content && (dpkg_content.error() != std::errc::no_such_file_or_directory)) {
        last_error = dpkg_content.error();
    }

    // 3. Check apk (/lib/apk/db/installed)
    const result<std::string> apk_content = linux_platform::read_text_file("/lib/apk/db/installed", 16U * 1024U * 1024U);
    if (apk_content && !apk_content->empty()) {
        any_source_found = true;
        std::vector<software_common::package_record> apk_pkgs;
        linux_impl::parse_apk_installed(*apk_content, apk_pkgs);
        for (auto& item : apk_pkgs) {
            add_unique_package(std::move(item));
        }
    } else if (!apk_content && (apk_content.error() != std::errc::no_such_file_or_directory)) {
        last_error = apk_content.error();
    }

    // 4. Also scan desktop application files in /usr/share/applications/, /usr/local/share/applications/, and ~/.local/share/applications/
    std::vector<std::string> app_dirs = {
        "/usr/share/applications",
        "/usr/local/share/applications"
    };
    const auto user_home_res = linux_impl::get_user_home_dir();
    if (!user_home_res) {
        return fail(user_home_res.error());
    }
    if (*user_home_res) {
        app_dirs.push_back(**user_home_res + "/.local/share/applications");
    }

    for (const auto& app_dir_path : app_dirs) {
        linux_platform::directory_handle app_dir(app_dir_path.c_str());
        if (!app_dir.valid()) {
            if (app_dir.error() != ENOENT) {
                last_error = std::error_code(app_dir.error(), std::generic_category());
            }
            continue;
        }
        any_source_found = true;

        for (;;) {
            errno = 0;
            struct dirent* entry = ::readdir(app_dir.get());
            if (entry == nullptr) {
                if (errno != 0) {
                    last_error = std::error_code(errno, std::generic_category());
                }
                break;
            }
            if (entry->d_name[0] == '.') {
                continue;
            }
            const std::string filename(entry->d_name);
            const std::string suffix = ".desktop";
            if (filename.size() <= suffix.size() ||
                filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0) {
                continue;
            }

            const std::string full_path = app_dir_path + "/" + filename;
            const result<std::string> content = linux_platform::read_text_file(full_path.c_str(), 64U * 1024U);
            if (!content) {
                if (content.error() != std::errc::no_such_file_or_directory) {
                    last_error = content.error();
                }
                continue;
            }

            software_common::package_record rec;
            if (linux_impl::parse_desktop_entry(*content, full_path, rec)) {
                add_unique_package(std::move(rec));
            }
        }
    }

    if (last_error) {
        return fail(last_error);
    }

    if (!any_source_found && pkgs.empty()) {
        return fail(errc::not_supported);
    }

    std::sort(pkgs.begin(), pkgs.end(), software_common::compare_packages);
    return pkgs;
}

inline result<software_common::package_record> find_package(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    const result<std::vector<software_common::package_record>> all = installed_packages();
    if (!all) {
        return fail(all.error());
    }
    for (const auto& item : *all) {
        if (item.name == name) {
            return item;
        }
    }
    return fail(errc::not_found);
}

} // namespace software_backend
} // namespace detail
} // namespace syscape

#endif
