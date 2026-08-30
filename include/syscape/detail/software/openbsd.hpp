#ifndef SYSCAPE_DETAIL_SOFTWARE_OPENBSD_HPP
#define SYSCAPE_DETAIL_SOFTWARE_OPENBSD_HPP

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

#include <syscape/software.hpp>
#include <syscape/detail/software/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace software_backend {

inline result<std::vector<software_common::service_record>> services() {
    std::vector<software_common::service_record> list;
    DIR* dir = ::opendir("/etc/rc.d");
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
    std::vector<software_common::package_record> packages;
    DIR* dir = ::opendir("/var/db/pkg");
    if (dir == nullptr) {
        if (errno == ENOENT) {
            return packages;
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
        std::string dirname(entry->d_name);
        if (!is_valid_utf8(dirname)) {
            ::closedir(dir);
            return fail(errc::invalid_encoding);
        }
        software_common::package_record pkg;
        std::size_t ver_pos = std::string::npos;
        for (std::size_t i = 0; i + 1 < dirname.size(); ++i) {
            if (dirname[i] == '-' &&
                std::isdigit(static_cast<unsigned char>(dirname[i + 1]))) {
                ver_pos = i;
                break;
            }
        }
        if (ver_pos != std::string::npos && ver_pos > 0) {
            pkg.name = dirname.substr(0, ver_pos);
            std::string rest = dirname.substr(ver_pos + 1);
            const std::size_t flavor_pos = rest.find('-');
            if (flavor_pos != std::string::npos) {
                pkg.version = rest.substr(0, flavor_pos);
            } else {
                pkg.version = std::move(rest);
            }
        } else {
            pkg.name = dirname;
        }
        pkg.format = software_common::package_format::unknown;
        packages.push_back(std::move(pkg));
        errno = 0;
    }
    if (errno != 0) {
        const int saved_errno = errno;
        ::closedir(dir);
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
    ::closedir(dir);

    std::sort(packages.begin(), packages.end(),
              [](const software_common::package_record& a,
                 const software_common::package_record& b) noexcept {
                  return a.name < b.name;
              });
    return packages;
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
