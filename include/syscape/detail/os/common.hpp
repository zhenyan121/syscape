#ifndef SYSCAPE_DETAIL_OS_COMMON_HPP
#define SYSCAPE_DETAIL_OS_COMMON_HPP

#include <string>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_common {

inline result<std::string> validate_utf8(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (!is_valid_utf8(*value)) { return fail(errc::malformed_data); }
    return value;
}

} // namespace os_common
} // namespace detail
} // namespace syscape

#endif
