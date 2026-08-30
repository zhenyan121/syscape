#ifndef SYSCAPE_DETAIL_SOFTWARE_FREEBSD_HPP
#define SYSCAPE_DETAIL_SOFTWARE_FREEBSD_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <string>
#include <string_view>
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/types.h>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/detail/software/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace software_backend {

inline result<std::vector<software_common::service_record>> services() {
    std::vector<software_common::service_record> list;
    static constexpr const char* const dirs[] = {"/etc/rc.d",
                                                 "/usr/local/etc/rc.d"};
    for (const char* dir_path : dirs) {
        DIR* dir = ::opendir(dir_path);
        if (dir == nullptr) {
            continue;
        }
        while (struct dirent* entry = ::readdir(dir)) {
            if (entry->d_name[0] == '.') {
                continue;
            }
            software_common::service_record rec;
            rec.name = entry->d_name;
            rec.display_name = entry->d_name;
            rec.state = software_common::service_state::unknown;
            rec.startup_type = software_common::startup_type::unknown;
            list.push_back(std::move(rec));
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
        if (s.name == name || s.display_name == name) {
            return s;
        }
    }
    return fail(errc::not_found);
}

inline result<std::vector<software_common::driver_record>> loaded_drivers() {
    std::vector<software_common::driver_record> drivers;
    for (int fileid = ::kldnext(0); fileid > 0; fileid = ::kldnext(fileid)) {
        struct kld_file_stat stat {};
        stat.version = sizeof(struct kld_file_stat);
        if (::kldstat(fileid, &stat) == 0) {
            software_common::driver_record drv;
            drv.name = stat.name;
            drv.size_bytes = static_cast<std::uint64_t>(stat.size);
            drv.use_count = static_cast<std::uint32_t>(stat.refs);
            drv.state = software_common::driver_state::loaded;
            if (stat.pathname[0] != '\0') {
                drv.path = stat.pathname;
            }
            drivers.push_back(std::move(drv));
        }
    }
    std::sort(drivers.begin(), drivers.end(),
              [](const software_common::driver_record& a,
                 const software_common::driver_record& b) noexcept {
                  return a.name < b.name;
              });
    return drivers;
}

inline result<software_common::driver_record>
find_driver(std::string_view name) {
    const result<std::vector<software_common::driver_record>> list =
        loaded_drivers();
    if (!list) {
        return fail(list.error());
    }
    for (const auto& d : *list) {
        if (d.name == name) {
            return d;
        }
    }
    return fail(errc::not_found);
}

inline result<std::vector<software_common::package_record>>
installed_packages() {
    return fail(errc::not_supported);
}

inline result<software_common::package_record> find_package(std::string_view) {
    return fail(errc::not_supported);
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
