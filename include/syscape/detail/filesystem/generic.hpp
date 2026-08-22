#ifndef SYSCAPE_DETAIL_FILESYSTEM_GENERIC_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_GENERIC_HPP

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

inline result<filesystem_common::space_snapshot> space(
    const std::string& /*path*/) {
    return fail(errc::not_supported);
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
