#ifndef SYSCAPE_DETAIL_SOFTWARE_MACOS_HPP
#define SYSCAPE_DETAIL_SOFTWARE_MACOS_HPP

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

#include <CoreFoundation/CoreFoundation.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <syscape/detail/software/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace software_backend {
namespace macos_impl {

class cf_release_guard {
public:
    explicit cf_release_guard(CFTypeRef ref) noexcept : ref_(ref) {}
    cf_release_guard(const cf_release_guard&) = delete;
    cf_release_guard& operator=(const cf_release_guard&) = delete;
    ~cf_release_guard() {
        if (ref_ != nullptr) {
            CFRelease(ref_);
        }
    }

private:
    CFTypeRef ref_;
};

class dir_stream {
public:
    explicit dir_stream(const char* path) noexcept
        : dir_(::opendir(path)), error_(dir_ == nullptr ? errno : 0) {}
    dir_stream(const dir_stream&) = delete;
    dir_stream& operator=(const dir_stream&) = delete;
    ~dir_stream() {
        if (dir_ != nullptr) {
            ::closedir(dir_);
        }
    }
    ::DIR* get() const noexcept { return dir_; }
    int error() const noexcept { return error_; }
    explicit operator bool() const noexcept { return dir_ != nullptr; }

private:
    ::DIR* dir_;
    int error_;
};

class macos_file_descriptor {
public:
    explicit macos_file_descriptor(int fd) noexcept : fd_(fd) {}
    macos_file_descriptor(const macos_file_descriptor&) = delete;
    macos_file_descriptor& operator=(const macos_file_descriptor&) = delete;
    ~macos_file_descriptor() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    int get() const noexcept { return fd_; }

private:
    int fd_;
};

inline result<std::string> read_text_file(const char* path, std::size_t max_size = 512U * 1024U) {
    const int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const macos_file_descriptor guard(fd);

    char buffer[4096];
    std::string content;
    for (;;) {
        const ssize_t count = ::read(fd, buffer, sizeof(buffer));
        if (count > 0) {
            const std::size_t size = static_cast<std::size_t>(count);
            if (content.size() > max_size || size > max_size - content.size()) {
                return fail(errc::value_too_large);
            }
            content.append(buffer, size);
            continue;
        }
        if (count == 0) {
            return content;
        }
        if (errno != EINTR) {
            const int err = errno;
            return fail(std::error_code(err, std::generic_category()));
        }
    }
}

inline result<std::string> cfstring_to_utf8(CFStringRef str) {
    if (str == nullptr) {
        return fail(errc::malformed_data);
    }
    const char* cptr = CFStringGetCStringPtr(str, kCFStringEncodingUTF8);
    if (cptr != nullptr) {
        return std::string(cptr);
    }
    const CFIndex length = CFStringGetLength(str);
    const CFIndex encoded_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    if (encoded_size < 0) {
        return fail(errc::invalid_encoding);
    }
    const CFIndex max_size = encoded_size + 1;
    std::vector<char> buffer(static_cast<std::size_t>(max_size));
    if (CFStringGetCString(str, buffer.data(), max_size, kCFStringEncodingUTF8)) {
        return std::string(buffer.data());
    }
    return fail(errc::invalid_encoding);
}

inline result<std::optional<std::string>> get_plist_string(CFDictionaryRef dict, CFStringRef key) {
    if (dict == nullptr || key == nullptr) {
        return fail(errc::malformed_data);
    }
    const void* val = CFDictionaryGetValue(dict, key);
    if (val == nullptr) {
        return std::optional<std::string>{std::nullopt};
    }
    if (CFGetTypeID(val) != CFStringGetTypeID()) {
        return fail(errc::malformed_data);
    }
    const auto converted = cfstring_to_utf8(static_cast<CFStringRef>(val));
    if (!converted) {
        return fail(converted.error());
    }
    return std::optional<std::string>{*converted};
}

inline result<bool> get_plist_bool(CFDictionaryRef dict, CFStringRef key) {
    if (dict == nullptr || key == nullptr) {
        return fail(errc::malformed_data);
    }
    const void* val = CFDictionaryGetValue(dict, key);
    if (val == nullptr) {
        return false;
    }
    if (CFGetTypeID(val) != CFBooleanGetTypeID()) {
        return fail(errc::malformed_data);
    }
    return CFBooleanGetValue(static_cast<CFBooleanRef>(val));
}

inline result<bool> plist_has_truthy_or_dict(CFDictionaryRef dict, CFStringRef key) {
    if (dict == nullptr || key == nullptr) {
        return fail(errc::malformed_data);
    }
    const void* val = CFDictionaryGetValue(dict, key);
    if (val == nullptr) {
        return false;
    }
    const CFTypeID type = CFGetTypeID(val);
    if (type == CFBooleanGetTypeID()) {
        return CFBooleanGetValue(static_cast<CFBooleanRef>(val));
    }
    if (type == CFDictionaryGetTypeID()) {
        return true;
    }
    return fail(errc::malformed_data);
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

inline bool has_executable_file(
    const char* const* paths,
    std::size_t path_count,
    std::error_code& out_error) {
    std::error_code candidate_error;
    for (std::size_t i = 0; i < path_count; ++i) {
        struct stat info;
        if (::stat(paths[i], &info) != 0) {
            const int err = errno;
            if (err != ENOENT && err != ENOTDIR) {
                candidate_error = std::error_code(err, std::generic_category());
            }
            continue;
        }
        if (!S_ISREG(info.st_mode)) {
            continue;
        }
        if (::access(paths[i], X_OK) == 0) {
            return true;
        }
        const int err = errno;
        if (err != ENOENT && err != ENOTDIR) {
            candidate_error = std::error_code(err, std::generic_category());
        }
    }
    if (candidate_error) {
        out_error = candidate_error;
    }
    return false;
}

inline bool has_directory(const char* path, std::error_code& out_error) {
    struct stat info;
    if (::stat(path, &info) == 0) {
        return S_ISDIR(info.st_mode);
    }
    const int err = errno;
    if (err != ENOENT && err != ENOTDIR) {
        out_error = std::error_code(err, std::generic_category());
    }
    return false;
}

} // namespace macos_impl

inline result<std::vector<software_common::service_record>> services() {
    std::vector<std::string> daemon_dirs = {
        "/Library/LaunchDaemons",
        "/Library/LaunchAgents",
        "/System/Library/LaunchDaemons",
        "/System/Library/LaunchAgents"
    };

    const auto user_home_res = macos_impl::get_user_home_dir();
    if (!user_home_res) {
        return fail(user_home_res.error());
    }
    if (*user_home_res) {
        daemon_dirs.insert(daemon_dirs.begin(), **user_home_res + "/Library/LaunchAgents");
    }

    std::unordered_set<std::string> seen_names;
    std::vector<software_common::service_record> result_services;
    std::error_code last_error;
    bool any_opened = false;

    for (const auto& dir_path : daemon_dirs) {
        macos_impl::dir_stream dir(dir_path.c_str());
        if (!dir) {
            if (dir.error() != ENOENT) {
                last_error = std::error_code(dir.error(), std::generic_category());
            }
            continue;
        }
        any_opened = true;

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
            const std::string suffix = ".plist";
            if (filename.size() <= suffix.size() ||
                filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0) {
                continue;
            }

            const std::string full_path = dir_path + "/" + filename;
            const auto content = macos_impl::read_text_file(full_path.c_str(), 256U * 1024U);
            if (!content) {
                if (content.error() != std::errc::no_such_file_or_directory &&
                    content.error() != errc::value_too_large) {
                    last_error = content.error();
                }
                continue;
            }

            CFDataRef data = CFDataCreateWithBytesNoCopy(
                kCFAllocatorDefault,
                reinterpret_cast<const UInt8*>(content->data()),
                static_cast<CFIndex>(content->size()),
                kCFAllocatorNull);
            if (data == nullptr) {
                return fail(errc::resource_exhausted);
            }
            macos_impl::cf_release_guard data_guard(data);

            CFPropertyListRef plist = CFPropertyListCreateWithData(
                kCFAllocatorDefault,
                data,
                kCFPropertyListImmutable,
                nullptr,
                nullptr);
            if (plist == nullptr) {
                continue;
            }
            macos_impl::cf_release_guard plist_guard(plist);

            if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
                continue;
            }
            CFDictionaryRef dict = static_cast<CFDictionaryRef>(plist);

            const auto label = macos_impl::get_plist_string(dict, CFSTR("Label"));
            if (!label) {
                continue;
            }
            const std::string name = *label
                ? **label
                : filename.substr(0, filename.size() - suffix.size());

            if (seen_names.find(name) != seen_names.end()) {
                continue;
            }
            seen_names.insert(name);

            software_common::service_record rec;
            rec.name = name;
            rec.display_name = name;
            rec.state = software_common::service_state::unknown;

            const auto disabled = macos_impl::get_plist_bool(dict, CFSTR("Disabled"));
            const auto run_at_load = macos_impl::get_plist_bool(dict, CFSTR("RunAtLoad"));
            const auto keep_alive = macos_impl::plist_has_truthy_or_dict(dict, CFSTR("KeepAlive"));
            if (!disabled) {
                continue;
            }
            if (!run_at_load) {
                continue;
            }
            if (!keep_alive) {
                continue;
            }

            if (*disabled) {
                rec.startup_type = software_common::service_startup::disabled;
            } else if (*run_at_load || *keep_alive) {
                rec.startup_type = software_common::service_startup::automatic;
            } else {
                rec.startup_type = software_common::service_startup::manual;
            }

            const auto program = macos_impl::get_plist_string(dict, CFSTR("Program"));
            if (!program) {
                continue;
            }
            if (*program) {
                rec.executable_path = **program;
            }

            result_services.push_back(std::move(rec));
        }
    }

    if (last_error) {
        return fail(last_error);
    }

    if (!any_opened && result_services.empty()) {
        return fail(errc::not_supported);
    }

    std::sort(result_services.begin(), result_services.end(), software_common::compare_services);
    return result_services;
}

inline result<software_common::service_record> find_service(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    const auto all = services();
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

inline result<std::vector<software_common::driver_record>> loaded_drivers() {
    return fail(errc::not_supported);
}

inline result<software_common::driver_record> find_driver(std::string_view /*name*/) {
    return fail(errc::not_supported);
}

inline result<std::vector<software_common::package_record>> installed_packages() {
    std::vector<std::string> app_dirs = {
        "/Applications",
        "/System/Applications",
        "/System/Applications/Utilities"
    };

    const auto user_home_res = macos_impl::get_user_home_dir();
    if (!user_home_res) {
        return fail(user_home_res.error());
    }
    if (*user_home_res) {
        app_dirs.push_back(**user_home_res + "/Applications");
    }

    std::unordered_set<std::string> seen_keys;
    std::vector<software_common::package_record> apps;
    std::error_code last_error;
    bool any_opened = false;

    for (const auto& dir_path : app_dirs) {
        macos_impl::dir_stream dir(dir_path.c_str());
        if (!dir) {
            if (dir.error() != ENOENT) {
                last_error = std::error_code(dir.error(), std::generic_category());
            }
            continue;
        }
        any_opened = true;

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
            const std::string suffix = ".app";
            if (filename.size() <= suffix.size() ||
                filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0) {
                continue;
            }

            const std::string bundle_path = dir_path + "/" + filename;
            const std::string plist_path = bundle_path + "/Contents/Info.plist";

            software_common::package_record rec;
            rec.name = filename.substr(0, filename.size() - suffix.size());
            rec.install_location = bundle_path;
            rec.format = software_common::package_format::macos_bundle;

            const auto content = macos_impl::read_text_file(plist_path.c_str(), 256U * 1024U);
            if (content) {
                CFDataRef data = CFDataCreateWithBytesNoCopy(
                    kCFAllocatorDefault,
                    reinterpret_cast<const UInt8*>(content->data()),
                    static_cast<CFIndex>(content->size()),
                    kCFAllocatorNull);
                if (data != nullptr) {
                    macos_impl::cf_release_guard data_guard(data);
                    CFPropertyListRef plist = CFPropertyListCreateWithData(
                        kCFAllocatorDefault,
                        data,
                        kCFPropertyListImmutable,
                        nullptr,
                        nullptr);
                    if (plist == nullptr) {
                        continue;
                    }
                    macos_impl::cf_release_guard plist_guard(plist);
                    if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
                        continue;
                    }
                    CFDictionaryRef dict = static_cast<CFDictionaryRef>(plist);
                    const auto bundle_name = macos_impl::get_plist_string(dict, CFSTR("CFBundleName"));
                    if (!bundle_name) {
                        continue;
                    }
                    if (*bundle_name && !(**bundle_name).empty()) {
                        rec.name = **bundle_name;
                    }
                    const auto version = macos_impl::get_plist_string(dict, CFSTR("CFBundleShortVersionString"));
                    if (!version) {
                        continue;
                    }
                    if (*version) {
                        rec.version = **version;
                    }
                    const auto publisher = macos_impl::get_plist_string(dict, CFSTR("NSHumanReadableCopyright"));
                    if (!publisher) {
                        continue;
                    }
                    if (*publisher) {
                        rec.publisher = **publisher;
                    }
                } else {
                    return fail(errc::resource_exhausted);
                }
            } else if (content.error() != std::errc::no_such_file_or_directory &&
                       content.error() != errc::value_too_large) {
                last_error = content.error();
            }

            const std::string dedup_key = software_common::make_package_dedup_key(rec);
            if (seen_keys.find(dedup_key) != seen_keys.end()) {
                continue;
            }
            seen_keys.insert(dedup_key);

            apps.push_back(std::move(rec));
        }
    }

    if (last_error) {
        return fail(last_error);
    }

    if (!any_opened && apps.empty()) {
        return fail(errc::not_supported);
    }

    std::sort(apps.begin(), apps.end(), software_common::compare_packages);
    return apps;
}

inline result<software_common::package_record> find_package(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    const auto all = installed_packages();
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

namespace macos_impl {

inline std::string_view trim_whitespace_view(std::string_view sv) noexcept {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return sv;
}

inline bool parse_java_release_file(
    std::string_view content,
    std::string& out_version,
    std::string& out_implementor,
    std::string& out_arch) {
    std::size_t pos = 0;
    bool found_version = false;
    while (pos < content.size()) {
        const std::size_t next_line = content.find('\n', pos);
        const std::size_t len = (next_line == std::string_view::npos) ? content.size() - pos : next_line - pos;
        const std::string_view line = trim_whitespace_view(content.substr(pos, len));
        pos = (next_line == std::string_view::npos) ? content.size() : next_line + 1;

        if (line.empty() || line.front() == '#') {
            continue;
        }

        const std::size_t eq_pos = line.find('=');
        if (eq_pos == std::string_view::npos) {
            continue;
        }

        const std::string_view key = trim_whitespace_view(line.substr(0, eq_pos));
        std::string_view val = trim_whitespace_view(line.substr(eq_pos + 1));
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }

        if (key == "JAVA_VERSION" || (key == "JAVA_RUNTIME_VERSION" && !found_version)) {
            out_version = std::string(val);
            found_version = true;
        } else if (key == "IMPLEMENTOR") {
            out_implementor = std::string(val);
        } else if (key == "OS_ARCH") {
            out_arch = std::string(val);
        }
    }
    return found_version;
}

inline bool parse_rust_channel_manifest(
    std::string_view content,
    std::string& out_version) {
    const std::size_t rustc_pos = content.find("[pkg.rustc]");
    if (rustc_pos == std::string_view::npos) {
        return false;
    }
    const std::size_t ver_pos = content.find("version = \"", rustc_pos);
    if (ver_pos == std::string_view::npos) {
        return false;
    }
    const std::size_t val_start = ver_pos + 11; // length of 'version = "'
    const std::size_t val_end = content.find('"', val_start);
    if (val_end == std::string_view::npos || val_end <= val_start) {
        return false;
    }
    const std::string_view raw_ver = content.substr(val_start, val_end - val_start);
    const std::size_t space_pos = raw_ver.find(' ');
    const std::string_view clean_ver = (space_pos == std::string_view::npos) ? raw_ver : raw_ver.substr(0, space_pos);
    if (!clean_ver.empty() && std::isdigit(static_cast<unsigned char>(clean_ver.front()))) {
        out_version = std::string(clean_ver);
        return true;
    }
    return false;
}

inline bool parse_rust_toolchain_version(
    std::string_view toolchain_name,
    std::string_view manifest_or_version_content,
    std::string& out_version) {
    if (!manifest_or_version_content.empty()) {
        if (parse_rust_channel_manifest(manifest_or_version_content, out_version)) {
            return true;
        }
        const std::string_view trimmed = trim_whitespace_view(manifest_or_version_content);
        const std::size_t space_pos = trimmed.find(' ');
        const std::string_view first_tok = (space_pos == std::string_view::npos) ? trimmed : trimmed.substr(0, space_pos);
        if (!first_tok.empty() && std::isdigit(static_cast<unsigned char>(first_tok.front()))) {
            out_version = std::string(first_tok);
            return true;
        }
    }
    if (!toolchain_name.empty() && std::isdigit(static_cast<unsigned char>(toolchain_name.front()))) {
        const std::size_t dash_pos = toolchain_name.find('-');
        if (dash_pos != std::string_view::npos) {
            out_version = std::string(toolchain_name.substr(0, dash_pos));
            return true;
        }
    }
    if (toolchain_name.rfind("nightly", 0) == 0 && toolchain_name.size() >= 18 &&
        toolchain_name[7] == '-' && toolchain_name[12] == '-' && toolchain_name[15] == '-') {
        out_version = std::string(toolchain_name.substr(0, 18));
        return true;
    }
    return false;
}

inline bool parse_go_version_file(
    std::string_view content,
    std::string& out_version) {
    const std::size_t line_end = content.find_first_of("\r\n");
    const std::string_view first_line = content.substr(0, line_end);
    const std::string_view trimmed = trim_whitespace_view(first_line);
    if (trimmed.empty()) {
        return false;
    }
    const std::string_view version = trimmed.rfind("go", 0) == 0
        ? trimmed.substr(2)
        : trimmed;
    if (version.empty()) {
        return false;
    }
    out_version = std::string(version);
    return true;
}

inline void merge_update_record(
    software_common::update_record&& rec,
    std::unordered_set<std::string>& seen,
    std::vector<software_common::update_record>& updates) {
    if (rec.identifier.empty()) {
        return;
    }
    if (seen.insert(rec.identifier).second) {
        updates.push_back(std::move(rec));
        return;
    }
    for (auto& existing : updates) {
        if (existing.identifier != rec.identifier) {
            continue;
        }
        if ((existing.title.empty() || existing.title == existing.identifier) &&
            !rec.title.empty()) {
            existing.title = std::move(rec.title);
        }
        if (!existing.version && rec.version) {
            existing.version = std::move(rec.version);
        }
        if (existing.classification == software_common::update_classification::unknown) {
            existing.classification = rec.classification;
        }
        if (existing.severity == software_common::update_severity::unknown) {
            existing.severity = rec.severity;
        }
        existing.requires_reboot = existing.requires_reboot || rec.requires_reboot;
        if (!existing.description && rec.description) {
            existing.description = std::move(rec.description);
        }
        return;
    }
}

} // namespace macos_impl

inline result<std::vector<software_common::update_record>> system_updates() {
    std::vector<software_common::update_record> updates;
    std::unordered_set<std::string> seen;
    bool any_source_found = false;
    std::error_code last_error;

    auto add_update = [&](software_common::update_record&& rec) {
        macos_impl::merge_update_record(std::move(rec), seen, updates);
    };

    const char* plist_paths[] = {
        "/Library/Updates/index.plist",
        "/Library/Updates/ProductMetadata.plist"
    };

    for (const char* plist_path : plist_paths) {
        const auto content = macos_impl::read_text_file(plist_path, 256U * 1024U);
        if (!content) {
            if (content.error() != std::errc::no_such_file_or_directory) {
                last_error = content.error();
            }
            continue;
        }
        CFDataRef data = CFDataCreateWithBytesNoCopy(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(content->data()),
            static_cast<CFIndex>(content->size()),
            kCFAllocatorNull);
        if (data == nullptr) {
            return fail(errc::malformed_data);
        }
        macos_impl::cf_release_guard data_guard(data);
        CFPropertyListRef plist = CFPropertyListCreateWithData(
            kCFAllocatorDefault,
            data,
            kCFPropertyListImmutable,
            nullptr,
            nullptr);
        if (plist == nullptr) {
            return fail(errc::malformed_data);
        }
        macos_impl::cf_release_guard plist_guard(plist);
        if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
            return fail(errc::malformed_data);
        }
        any_source_found = true;
        CFDictionaryRef dict = static_cast<CFDictionaryRef>(plist);

        // Check ProductPaths dictionary
        const void* pp_val = CFDictionaryGetValue(dict, CFSTR("ProductPaths"));
        if (pp_val != nullptr && CFGetTypeID(pp_val) != CFDictionaryGetTypeID()) {
            return fail(errc::malformed_data);
        }
        if (pp_val != nullptr) {
            CFDictionaryRef pp_dict = static_cast<CFDictionaryRef>(pp_val);
            const CFIndex pp_count = CFDictionaryGetCount(pp_dict);
            if (pp_count > 0) {
                std::vector<const void*> pp_keys(static_cast<std::size_t>(pp_count));
                std::vector<const void*> pp_vals(static_cast<std::size_t>(pp_count));
                CFDictionaryGetKeysAndValues(pp_dict, pp_keys.data(), pp_vals.data());
                for (std::size_t i = 0; i < static_cast<std::size_t>(pp_count); ++i) {
                    if (pp_keys[i] == nullptr || CFGetTypeID(pp_keys[i]) != CFStringGetTypeID() ||
                        pp_vals[i] == nullptr || CFGetTypeID(pp_vals[i]) != CFStringGetTypeID()) {
                        return fail(errc::malformed_data);
                    }
                    const auto key_utf8 = macos_impl::cfstring_to_utf8(static_cast<CFStringRef>(pp_keys[i]));
                    if (!key_utf8) {
                        return fail(key_utf8.error());
                    }
                    if (!key_utf8->empty()) {
                        software_common::update_record rec;
                        rec.identifier = *key_utf8;
                        rec.title = *key_utf8;
                        add_update(std::move(rec));
                    }
                }
            }
        }

        // Check Products dictionary
        const void* prods_val = CFDictionaryGetValue(dict, CFSTR("Products"));
        if (prods_val != nullptr && CFGetTypeID(prods_val) != CFDictionaryGetTypeID()) {
            return fail(errc::malformed_data);
        }
        if (prods_val != nullptr) {
            CFDictionaryRef prods_dict = static_cast<CFDictionaryRef>(prods_val);
            const CFIndex prods_count = CFDictionaryGetCount(prods_dict);
            if (prods_count > 0) {
                std::vector<const void*> p_keys(static_cast<std::size_t>(prods_count));
                std::vector<const void*> p_vals(static_cast<std::size_t>(prods_count));
                CFDictionaryGetKeysAndValues(prods_dict, p_keys.data(), p_vals.data());
                for (std::size_t i = 0; i < static_cast<std::size_t>(prods_count); ++i) {
                    if (p_keys[i] == nullptr || CFGetTypeID(p_keys[i]) != CFStringGetTypeID() ||
                        p_vals[i] == nullptr || CFGetTypeID(p_vals[i]) != CFDictionaryGetTypeID()) {
                        return fail(errc::malformed_data);
                    }
                    const auto key_utf8 = macos_impl::cfstring_to_utf8(static_cast<CFStringRef>(p_keys[i]));
                    if (!key_utf8) {
                        return fail(key_utf8.error());
                    }
                    if (!key_utf8->empty()) {
                        software_common::update_record rec;
                        rec.identifier = *key_utf8;
                        rec.title = *key_utf8;
                        CFDictionaryRef sub_dict = static_cast<CFDictionaryRef>(p_vals[i]);
                        const auto title_res = macos_impl::get_plist_string(sub_dict, CFSTR("title"));
                        if (!title_res) {
                            return fail(title_res.error());
                        }
                        if (*title_res && !(**title_res).empty()) {
                            rec.title = **title_res;
                        }
                        const auto ver_res = macos_impl::get_plist_string(sub_dict, CFSTR("version"));
                        if (!ver_res) {
                            return fail(ver_res.error());
                        }
                        if (*ver_res && !(**ver_res).empty()) {
                            rec.version = **ver_res;
                        }
                        const auto restart_res = macos_impl::get_plist_string(sub_dict, CFSTR("RestartAction"));
                        if (!restart_res) {
                            return fail(restart_res.error());
                        }
                        if (*restart_res &&
                            (**restart_res == "RequireRestart" || **restart_res == "RestartRequired")) {
                            rec.requires_reboot = true;
                        }
                        add_update(std::move(rec));
                    }
                }
            }
        }
    }

    if (last_error) {
        return fail(last_error);
    }

    if (!any_source_found && updates.empty()) {
        return fail(errc::not_supported);
    }

    std::sort(updates.begin(), updates.end(), software_common::compare_updates);
    return updates;
}

inline result<std::vector<software_common::runtime_record>> installed_runtimes() {
    std::vector<software_common::runtime_record> runtimes;
    std::unordered_set<std::string> seen;
    std::error_code last_error;

    auto add_runtime = [&](software_common::runtime_record&& rec) {
        if (rec.version.empty() || rec.installation_path.empty()) {
            return;
        }
        const std::string key = software_common::make_runtime_dedup_key(rec);
        if (seen.insert(key).second) {
            runtimes.push_back(std::move(rec));
        }
    };

    // 1. Java in /Library/Java/JavaVirtualMachines
    const char* jvm_dir_path = "/Library/Java/JavaVirtualMachines";
    DIR* jvm_dir = ::opendir(jvm_dir_path);
    if (jvm_dir != nullptr) {
        for (;;) {
            errno = 0;
            struct dirent* entry = ::readdir(jvm_dir);
            if (entry == nullptr) {
                if (errno != 0) {
                    last_error = std::error_code(errno, std::generic_category());
                }
                break;
            }
            if (entry->d_name[0] == '.') {
                continue;
            }
            const std::string jvm_name(entry->d_name);
            const std::string full_path = std::string(jvm_dir_path) + "/" + jvm_name;
            const std::string java_bin1 = full_path + "/Contents/Home/bin/java";
            const std::string java_bin2 = full_path + "/bin/java";
            const char* java_bins[] = { java_bin1.c_str(), java_bin2.c_str() };
            if (!macos_impl::has_executable_file(
                    java_bins, sizeof(java_bins) / sizeof(java_bins[0]), last_error)) {
                continue;
            }
            const std::string release_file = full_path + "/Contents/Home/release";
            const auto content = macos_impl::read_text_file(release_file.c_str(), 16U * 1024U);
            if (content && !content->empty()) {
                std::string version, implementor, arch;
                if (macos_impl::parse_java_release_file(*content, version, implementor, arch)) {
                    software_common::runtime_record rec;
                    rec.kind = software_common::runtime_kind::java;
                    rec.name = implementor.empty() ? ("Java (" + jvm_name + ")") : ("Java (" + implementor + ")");
                    rec.version = version;
                    rec.installation_path = full_path;
                    if (!arch.empty()) {
                        rec.architecture = arch;
                    }
                    add_runtime(std::move(rec));
                }
            } else if (!content && content.error() != std::errc::no_such_file_or_directory) {
                last_error = content.error();
            }
        }
        ::closedir(jvm_dir);
    } else if (errno != ENOENT) {
        last_error = std::error_code(errno, std::generic_category());
    }

    // 2. Python in /Library/Frameworks/Python.framework/Versions
    const char* py_framework_dir = "/Library/Frameworks/Python.framework/Versions";
    DIR* py_dir = ::opendir(py_framework_dir);
    if (py_dir != nullptr) {
        for (;;) {
            errno = 0;
            struct dirent* entry = ::readdir(py_dir);
            if (entry == nullptr) {
                if (errno != 0) {
                    last_error = std::error_code(errno, std::generic_category());
                }
                break;
            }
            if (entry->d_name[0] == '.' || std::strcmp(entry->d_name, "Current") == 0) {
                continue;
            }
            const std::string ver(entry->d_name);
            const std::string full_path = std::string(py_framework_dir) + "/" + ver;
            const std::string py_bin1 = full_path + "/bin/python3";
            const std::string py_bin2 = full_path + "/bin/python";
            const char* python_bins[] = { py_bin1.c_str(), py_bin2.c_str() };
            if (!macos_impl::has_executable_file(
                    python_bins, sizeof(python_bins) / sizeof(python_bins[0]), last_error)) {
                continue;
            }
            software_common::runtime_record rec;
            rec.kind = software_common::runtime_kind::python;
            rec.name = "Python";
            rec.version = ver;
            rec.installation_path = full_path;
            add_runtime(std::move(rec));
        }
        ::closedir(py_dir);
    } else if (errno != ENOENT) {
        last_error = std::error_code(errno, std::generic_category());
    }

    // 3. .NET in /usr/local/share/dotnet/shared/Microsoft.NETCore.App
    const char* dotnet_shared = "/usr/local/share/dotnet/shared/Microsoft.NETCore.App";
    const char* dotnet_bins[] = { "/usr/local/share/dotnet/dotnet" };
    if (macos_impl::has_executable_file(dotnet_bins, 1, last_error)) {
        DIR* dot_dir = ::opendir(dotnet_shared);
        if (dot_dir != nullptr) {
            for (;;) {
                errno = 0;
                struct dirent* entry = ::readdir(dot_dir);
                if (entry == nullptr) {
                    if (errno != 0) {
                        last_error = std::error_code(errno, std::generic_category());
                    }
                    break;
                }
                if (entry->d_name[0] == '.') {
                    continue;
                }
                const std::string ver(entry->d_name);
                const std::string full_path = std::string(dotnet_shared) + "/" + ver;
                if (!macos_impl::has_directory(full_path.c_str(), last_error)) {
                    continue;
                }
                software_common::runtime_record rec;
                rec.kind = software_common::runtime_kind::dotnet;
                rec.name = ".NET Runtime";
                rec.version = ver;
                rec.installation_path = full_path;
                add_runtime(std::move(rec));
            }
            ::closedir(dot_dir);
        } else if (errno != ENOENT) {
            last_error = std::error_code(errno, std::generic_category());
        }
    }

    // 4. Go in /usr/local/go/VERSION
    const char* go_bins[] = { "/usr/local/go/bin/go" };
    if (macos_impl::has_executable_file(go_bins, 1, last_error)) {
        const auto go_ver = macos_impl::read_text_file("/usr/local/go/VERSION", 4U * 1024U);
        if (go_ver && !go_ver->empty()) {
            std::string ver;
            if (macos_impl::parse_go_version_file(*go_ver, ver)) {
                software_common::runtime_record rec;
                rec.kind = software_common::runtime_kind::golang;
                rec.name = "Go";
                rec.version = ver;
                rec.installation_path = "/usr/local/go";
                add_runtime(std::move(rec));
            }
        } else if (!go_ver && go_ver.error() != std::errc::no_such_file_or_directory) {
            last_error = go_ver.error();
        }
    }

    // 5. Rust in ~/.rustup/toolchains
    const auto home_res = macos_impl::get_user_home_dir();
    if (home_res && *home_res) {
        const std::string rust_path = **home_res + "/.rustup/toolchains";
        DIR* rdir = ::opendir(rust_path.c_str());
        if (rdir != nullptr) {
            for (;;) {
                errno = 0;
                struct dirent* entry = ::readdir(rdir);
                if (entry == nullptr) {
                    if (errno != 0) {
                        last_error = std::error_code(errno, std::generic_category());
                    }
                    break;
                }
                if (entry->d_name[0] == '.') {
                    continue;
                }
                const std::string toolchain(entry->d_name);
                const std::string full_path = rust_path + "/" + toolchain;
                const std::string rustc_bin = full_path + "/bin/rustc";
                const char* rustc_bins[] = { rustc_bin.c_str() };
                if (!macos_impl::has_executable_file(rustc_bins, 1, last_error)) {
                    continue;
                }
                const std::string manifest_file = full_path + "/lib/rustlib/multirust-channel-manifest.toml";
                const auto manifest_content = macos_impl::read_text_file(manifest_file.c_str(), 1024U * 1024U);
                if (!manifest_content && manifest_content.error() != std::errc::no_such_file_or_directory) {
                    last_error = manifest_content.error();
                }
                std::string parsed_version;
                const std::string_view mview = manifest_content ? std::string_view(*manifest_content) : std::string_view{};
                if (!macos_impl::parse_rust_toolchain_version(toolchain, mview, parsed_version)) {
                    const std::string ver_file = full_path + "/version";
                    const auto ver_content = macos_impl::read_text_file(ver_file.c_str(), 4U * 1024U);
                    if (!ver_content && ver_content.error() != std::errc::no_such_file_or_directory) {
                        last_error = ver_content.error();
                    }
                    const std::string_view vview = ver_content ? std::string_view(*ver_content) : std::string_view{};
                    macos_impl::parse_rust_toolchain_version(toolchain, vview, parsed_version);
                }
                if (!parsed_version.empty()) {
                    software_common::runtime_record rec;
                    rec.kind = software_common::runtime_kind::rust;
                    rec.name = "Rust (" + toolchain + ")";
                    rec.version = parsed_version;
                    rec.installation_path = full_path;
                    add_runtime(std::move(rec));
                }
            }
            ::closedir(rdir);
        } else if (errno != ENOENT) {
            last_error = std::error_code(errno, std::generic_category());
        }
    } else if (!home_res) {
        last_error = home_res.error();
    }

    if (last_error) {
        return fail(last_error);
    }

    std::sort(runtimes.begin(), runtimes.end(), software_common::compare_runtimes);
    return runtimes;
}

} // namespace software_backend
} // namespace detail
} // namespace syscape

#endif
