#ifndef SYSCAPE_DETAIL_FILESYSTEM_DOS_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_DOS_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

inline result<std::vector<filesystem_common::mount_record>> mounts() {
    return fail(errc::not_supported);
}

inline result<filesystem_common::space_snapshot>
space(const std::string& /*path*/) {
    return fail(errc::not_supported);
}

inline result<filesystem_common::path_length_snapshot>
max_component_length(const std::string& /*path*/) {
    filesystem_common::path_length_snapshot snap;
    snap.length = 12U; // DOS standard 8.3 format
    snap.indeterminate = false;
    return snap;
}

inline result<filesystem_common::path_length_snapshot>
max_path_length(const std::string& /*path*/) {
    filesystem_common::path_length_snapshot snap;
    snap.length = 80U; // DOS standard path limit
    snap.indeterminate = false;
    return snap;
}

inline result<std::string> volume_id(const std::string& /*path*/) {
    return fail(errc::not_supported);
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
