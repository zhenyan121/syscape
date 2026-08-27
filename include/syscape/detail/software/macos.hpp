#ifndef SYSCAPE_DETAIL_SOFTWARE_MACOS_HPP
#define SYSCAPE_DETAIL_SOFTWARE_MACOS_HPP

#include <algorithm>
#include <cstdint>
#include <cstdlib>
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

} // namespace software_backend
} // namespace detail
} // namespace syscape

#endif
