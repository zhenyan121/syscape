#ifndef SYSCAPE_DETAIL_FILESYSTEM_IBMI_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_IBMI_HPP

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
#if defined(SYSCAPE_IBMI_QSYS)
    snap.length = 10U; // Classic QSYS object / member name limit
#else
    snap.length = 255U; // Standard IFS NAME_MAX
#endif
    snap.indeterminate = false;
    return snap;
}

inline result<filesystem_common::path_length_snapshot>
max_path_length(const std::string& /*path*/) {
    filesystem_common::path_length_snapshot snap;
#if defined(SYSCAPE_IBMI_QSYS)
    snap.length = 33U; // /QSYS.LIB/LIBNAME.LIB/OBJ.FILE
#elif defined(SYSCAPE_IBMI_PATH_MAX)
    snap.length = static_cast<std::uint64_t>(SYSCAPE_IBMI_PATH_MAX);
#else
    snap.length = 5000U; // Integrated File System (IFS) PATH_MAX
#endif
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
