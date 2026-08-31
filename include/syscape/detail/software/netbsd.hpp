#ifndef SYSCAPE_DETAIL_SOFTWARE_NETBSD_HPP
#define SYSCAPE_DETAIL_SOFTWARE_NETBSD_HPP

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/detail/software/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>
#include <syscape/software.hpp>

namespace syscape {
namespace detail {
namespace software_backend {

inline result<std::vector<software_common::service_record>> services() {
    std::vector<software_common::service_record> list;
    const char* paths[] = {"/etc/rc.d", "/usr/pkg/etc/rc.d"};

    for (const char* path : paths) {
        DIR* dir = ::opendir(path);
        if (dir == nullptr) {
            if (errno == ENOENT) {
                continue;
            }
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }

        errno = 0;
        while (struct dirent* entry = ::readdir(dir)) {
            if (entry->d_name[0] == '.') {
                continue;
            }
            std::string name(entry->d_name);
            if (!is_valid_utf8(name)) {
                ::closedir(dir);
                return fail(errc::invalid_encoding);
            }
            software_common::service_record rec;
            rec.name = name;
            rec.display_name = name;
            rec.state = software_common::service_state::unknown;
            rec.startup_type = software_common::service_startup::unknown;
            list.push_back(std::move(rec));
            errno = 0;
        }
        if (errno != 0) {
            const int saved_errno = errno;
            ::closedir(dir);
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        ::closedir(dir);
    }

    std::sort(list.begin(), list.end(),
              [](const software_common::service_record& a,
                 const software_common::service_record& b) noexcept {
                  return a.name < b.name;
              });
    return list;
}

inline result<software_common::service_record>
find_service(std::string_view name) {
    const result<std::vector<software_common::service_record>> list =
        services();
    if (!list) {
        return fail(list.error());
    }
    for (const auto& s : *list) {
        if (s.name == name || (s.display_name && *s.display_name == name)) {
            return s;
        }
    }
    return fail(errc::not_found);
}

inline result<std::vector<software_common::driver_record>> loaded_drivers() {
    return fail(errc::not_supported);
}

inline result<software_common::driver_record>
find_driver(std::string_view name) {
    static_cast<void>(name);
    return fail(errc::not_supported);
}

inline result<std::vector<software_common::package_record>>
installed_packages() {
    std::vector<software_common::package_record> list;
    DIR* dir = ::opendir("/var/db/pkg");
    if (dir == nullptr) {
        if (errno == ENOENT) {
            return list;
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    errno = 0;
    while (struct dirent* entry = ::readdir(dir)) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) {
            continue;
        }
        std::string raw_name(entry->d_name);
        if (raw_name == "pkgdb.byfile.db" || raw_name == "pkgin") {
            continue;
        }
        if (!is_valid_utf8(raw_name)) {
            ::closedir(dir);
            return fail(errc::invalid_encoding);
        }

        software_common::package_record rec;

        const std::size_t last_dash = raw_name.rfind('-');
        if (last_dash != std::string::npos && last_dash > 0 &&
            last_dash + 1 < raw_name.size() &&
            std::isdigit(static_cast<unsigned char>(raw_name[last_dash + 1]))) {
            rec.name = raw_name.substr(0, last_dash);
            rec.version = raw_name.substr(last_dash + 1);
        } else {
            rec.name = raw_name;
        }
        rec.format = software_common::package_format::unknown;

        list.push_back(std::move(rec));
        errno = 0;
    }
    if (errno != 0) {
        const int saved_errno = errno;
        ::closedir(dir);
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
    ::closedir(dir);

    std::sort(list.begin(), list.end(),
              [](const software_common::package_record& a,
                 const software_common::package_record& b) noexcept {
                  return a.name < b.name;
              });
    return list;
}

inline result<software_common::package_record>
find_package(std::string_view name) {
    const auto list = installed_packages();
    if (!list) {
        return fail(list.error());
    }
    for (const auto& pkg : *list) {
        if (pkg.name == name) {
            return pkg;
        }
    }
    return fail(errc::not_found);
}

inline result<std::vector<software_common::update_record>> system_updates() {
    return fail(errc::not_supported);
}

inline result<std::vector<software_common::runtime_record>>
installed_runtimes() {
    return fail(errc::not_supported);
}

} // namespace software_backend
} // namespace detail
} // namespace syscape

#endif
