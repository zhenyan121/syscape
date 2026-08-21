#ifndef SYSCAPE_DETAIL_CPU_COMMON_HPP
#define SYSCAPE_DETAIL_CPU_COMMON_HPP

#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_common {

inline result<std::vector<std::string>> validate_utf8_labels(
    result<std::vector<std::string>> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty()) { return fail(errc::malformed_data); }
    for (const std::string& label : *value) {
        if (label.empty() || !is_valid_utf8(label)) {
            return fail(errc::malformed_data);
        }
    }
    return value;
}

} // namespace cpu_common
} // namespace detail
} // namespace syscape

#endif
