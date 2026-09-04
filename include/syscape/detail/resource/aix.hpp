#ifndef SYSCAPE_DETAIL_RESOURCE_AIX_HPP
#define SYSCAPE_DETAIL_RESOURCE_AIX_HPP

#include <cstdlib>
#include <cstdint>
#include <sys/resource.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<libperfstat.h>)
#include <libperfstat.h>
#define SYSCAPE_HAS_AIX_LIBPERFSTAT 1
#endif
#endif

#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

inline result<resource_common::load_samples> load_average() {
    double samples[3] = {0.0, 0.0, 0.0};
    if (::getloadavg(samples, 3) == 3) {
        resource_common::load_samples result {};
        result.one_minute = samples[0];
        result.five_minute = samples[1];
        result.fifteen_minute = samples[2];
        return result;
    }
    return fail(errc::not_supported);
}

inline result<resource_common::entity_counts> scheduler_entities() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> process_count() {
#if defined(SYSCAPE_HAS_AIX_LIBPERFSTAT)
    const int count =
        ::perfstat_process(nullptr, nullptr, sizeof(perfstat_process_t), 0);
    if (count > 0) {
        return static_cast<std::uint64_t>(count);
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint64_t> thread_count() {
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
