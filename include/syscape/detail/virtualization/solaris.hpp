#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_SOLARIS_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_SOLARIS_HPP

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#if defined(__has_include)
#if __has_include(<zone.h>)
#include <zone.h>
#define SYSCAPE_HAS_ZONE 1
#endif
#if __has_include(<sys/zone.h>)
#include <sys/zone.h>
#endif
#endif

#include <syscape/detail/virtualization/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace virtualization_backend {

inline result<bool> is_hypervisor_present() {
    return fail(errc::not_supported);
}

inline result<virtualization_common::hypervisor_type> hypervisor() {
    return fail(errc::not_supported);
}

inline result<std::string> hypervisor_name() {
    return fail(errc::not_supported);
}

inline result<std::string> query_zone_name(::zoneid_t zid) {
    std::size_t size = 64U;
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::string buffer(size, '\0');
        errno = 0;
        const ssize_t len = ::getzonenamebyid(zid, &buffer[0], size);
        if (len < 0) {
            const int err = errno != 0 ? errno : EIO;
            if (err == ENOENT) {
                return fail(errc::not_found);
            }
            if (err == ENOSYS) {
                return fail(errc::not_supported);
            }
            return fail(std::error_code(err, std::generic_category()));
        }
        if (static_cast<std::size_t>(len) <= size) {
            buffer.resize(std::strlen(buffer.c_str()));
            if (buffer.empty()) {
                return fail(errc::malformed_data);
            }
            return buffer;
        }
        size = static_cast<std::size_t>(len);
    }
    return fail(errc::malformed_data);
}

inline result<std::string> query_zone_brand(::zoneid_t zid) {
#if defined(ZONE_ATTR_BRAND)
    std::size_t size = 64U;
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::string buffer(size, '\0');
        errno = 0;
        const ssize_t len =
            ::zone_getattr(zid, ZONE_ATTR_BRAND, &buffer[0], size);
        if (len < 0) {
            const int err = errno != 0 ? errno : EIO;
            if (err == EINVAL || err == ENOSYS || err == ENOTSUP ||
                err == ENOENT) {
                return fail(errc::not_supported);
            }
            return fail(std::error_code(err, std::generic_category()));
        }
        if (static_cast<std::size_t>(len) <= size) {
            buffer.resize(std::strlen(buffer.c_str()));
            return buffer;
        }
        size = static_cast<std::size_t>(len);
    }
    return fail(errc::malformed_data);
#else
    static_cast<void>(zid);
    return fail(errc::not_supported);
#endif
}

inline result<bool> is_container() {
#if defined(SYSCAPE_HAS_ZONE)
    errno = 0;
    const ::zoneid_t zid = ::getzoneid();
    if (zid < 0) {
        const int err = errno != 0 ? errno : EIO;
        return fail(std::error_code(err, std::generic_category()));
    }
#if defined(GLOBAL_ZONEID)
    if (zid != GLOBAL_ZONEID) {
        return true;
    }
#else
    if (zid > 0) {
        return true;
    }
#endif

    const auto brand = query_zone_brand(zid);
    if (!brand && brand.error() != errc::not_supported) {
        return fail(brand.error());
    }
    if (brand && *brand == "solaris-kz") {
        return true;
    }

    const auto name = query_zone_name(zid);
    if (!name) {
        return fail(name.error());
    }
    if (*name != "global") {
        return true;
    }

    return false;
#else
    return fail(errc::not_supported);
#endif
}

inline result<virtualization_common::container_type> container() {
    const auto inside = is_container();
    if (!inside) {
        return fail(inside.error());
    }
    if (*inside) {
        return virtualization_common::container_type::other;
    }
    return virtualization_common::container_type::none;
}

inline result<std::string> container_name() {
#if defined(SYSCAPE_HAS_ZONE)
    const auto inside = is_container();
    if (!inside) {
        return fail(inside.error());
    }
    if (!*inside) {
        return fail(errc::not_found);
    }
    const ::zoneid_t zid = ::getzoneid();
    if (zid < 0) {
        const int err = errno != 0 ? errno : EIO;
        return fail(std::error_code(err, std::generic_category()));
    }

    const auto brand = query_zone_brand(zid);
    if (!brand && brand.error() != errc::not_supported) {
        return fail(brand.error());
    }
    if (brand && *brand == "solaris-kz") {
        return fail(errc::not_supported);
    }

    const auto name = query_zone_name(zid);
    if (!name) {
        return fail(name.error());
    }
    if (*name == "global") {
        return fail(errc::not_supported);
    }
    return *name;
#else
    return fail(errc::not_supported);
#endif
}

inline result<bool> is_wsl() {
    return false;
}

inline result<std::uint32_t> wsl_version() {
    return fail(errc::not_supported);
}

inline result<bool> is_sandboxed() {
    return fail(errc::not_supported);
}

inline result<virtualization_common::sandbox_type> sandbox() {
    return fail(errc::not_supported);
}

inline result<virtualization_common::cgroup_version_type>
cgroup_hierarchy_version() {
    return fail(errc::not_supported);
}

inline result<virtualization_common::cgroup_record> current_cgroup() {
    return fail(errc::not_supported);
}

inline result<std::vector<virtualization_common::namespace_record>>
namespaces() {
    return fail(errc::not_supported);
}

inline result<bool> is_namespace_isolated() {
    return fail(errc::not_supported);
}

} // namespace virtualization_backend
} // namespace detail
} // namespace syscape

#endif
