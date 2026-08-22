#ifndef SYSCAPE_DETAIL_PROCESS_COMMON_HPP
#define SYSCAPE_DETAIL_PROCESS_COMMON_HPP

#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_common {

inline result<std::string> validate_utf8_path(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty() || !is_valid_utf8(*value)) {
        return value->empty() ? fail(errc::malformed_data)
                              : fail(errc::invalid_encoding);
    }
    return value;
}

inline result<std::vector<std::string>> validate_utf8_arguments(
    result<std::vector<std::string>> value) {
    if (!value) { return fail(value.error()); }
    for (const std::string& argument : *value) {
        if (!is_valid_utf8(argument)) { return fail(errc::invalid_encoding); }
    }
    return value;
}

} // namespace process_common
} // namespace detail
} // namespace syscape

#endif
