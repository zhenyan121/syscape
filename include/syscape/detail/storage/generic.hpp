#ifndef SYSCAPE_DETAIL_STORAGE_GENERIC_HPP
#define SYSCAPE_DETAIL_STORAGE_GENERIC_HPP

#include <vector>

#include <syscape/detail/storage/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace storage_backend {

inline result<std::vector<storage_common::drive_record>> drives() {
    return fail(errc::not_supported);
}

inline result<std::vector<storage_common::partition_record>> partitions() {
    return fail(errc::not_supported);
}

inline result<storage_common::health_record> health(std::string_view) {
    return fail(errc::not_supported);
}

inline result<std::vector<storage_common::health_record>> all_drive_health() {
    return fail(errc::not_supported);
}

} // namespace storage_backend
} // namespace detail
} // namespace syscape

#endif
