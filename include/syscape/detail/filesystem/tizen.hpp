#ifndef SYSCAPE_DETAIL_FILESYSTEM_TIZEN_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_TIZEN_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/filesystem/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

template <typename T>
inline result<std::uint64_t> validate_positive_u64_len(T value) {
    if constexpr (std::is_signed<T>::value) {
        if (value <= 0) {
            return fail(errc::malformed_data);
        }
    } else {
        if (value == 0) {
            return fail(errc::malformed_data);
        }
    }
    return static_cast<std::uint64_t>(value);
}

/// Live mount table enumeration is not exposed in unprivileged sandboxes.
inline result<std::vector<filesystem_common::mount_record>> mounts() {
    return fail(errc::not_supported);
}

/// Filesystem space queries are not exposed in unprivileged sandboxes.
inline result<filesystem_common::space_snapshot>
space(const std::string& /*path*/) {
    return fail(errc::not_supported);
}

/// Returns the maximum component length via override or POSIX pathconf.
inline result<filesystem_common::path_length_snapshot>
max_component_length(const std::string& path) {
    (void)path;
#if defined(SYSCAPE_TIZEN_NAME_MAX)
    const auto len = validate_positive_u64_len(SYSCAPE_TIZEN_NAME_MAX);
    if (!len) {
        return fail(len.error());
    }
    filesystem_common::path_length_snapshot snap;
    snap.length = *len;
    snap.indeterminate = false;
    return snap;
#else
    return pathconf_limit(path, _PC_NAME_MAX);
#endif
}

/// Returns the maximum path length via override or POSIX pathconf.
inline result<filesystem_common::path_length_snapshot>
max_path_length(const std::string& path) {
    (void)path;
#if defined(SYSCAPE_TIZEN_PATH_MAX)
    const auto len = validate_positive_u64_len(SYSCAPE_TIZEN_PATH_MAX);
    if (!len) {
        return fail(len.error());
    }
    filesystem_common::path_length_snapshot snap;
    snap.length = *len;
    snap.indeterminate = false;
    return snap;
#else
    return pathconf_limit(path, _PC_PATH_MAX);
#endif
}

/// Volume identifier query is not supported in unprivileged sandboxes.
inline result<std::string> volume_id(const std::string& /*path*/) {
    return fail(errc::not_supported);
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
