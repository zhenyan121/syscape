#ifndef SYSCAPE_DETAIL_RESOURCE_HAIKU_HPP
#define SYSCAPE_DETAIL_RESOURCE_HAIKU_HPP

#include <cstdint>
#include <cstdlib>
#include <sys/resource.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<OS.h>)
#include <OS.h>
#define SYSCAPE_HAS_HAIKU_OS_H 1
#endif
#endif

#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

inline result<resource_common::load_samples> load_average() {
    double samples[3] = {0.0, 0.0, 0.0};
    if (::getloadavg(samples, 3) != 3) {
        return fail(errc::io_error);
    }
    resource_common::load_samples loads;
    loads.one_minute = samples[0];
    loads.five_minute = samples[1];
    loads.fifteen_minute = samples[2];
    return loads;
}

inline result<resource_common::entity_counts> scheduler_entities() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> process_count() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info info {};
    if (::get_system_info(&info) == B_OK && info.used_teams > 0) {
        return static_cast<std::uint64_t>(info.used_teams);
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint64_t> thread_count() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info info {};
    if (::get_system_info(&info) == B_OK && info.used_threads > 0) {
        return static_cast<std::uint64_t>(info.used_threads);
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint64_t> open_file_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> open_handle_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> file_descriptor_limit() {
    struct ::rlimit rl {};
    if (::getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur > 0) {
        return static_cast<std::uint64_t>(rl.rlim_cur);
    }
    return fail(errc::not_supported);
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
