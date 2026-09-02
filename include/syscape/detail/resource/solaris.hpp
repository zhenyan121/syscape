#ifndef SYSCAPE_DETAIL_RESOURCE_SOLARIS_HPP
#define SYSCAPE_DETAIL_RESOURCE_SOLARIS_HPP

#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <sys/loadavg.h>
#include <system_error>
#include <sys/resource.h>
#include <unistd.h>

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
    DIR* dir = ::opendir("/proc");
    if (dir == nullptr) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    struct dir_guard {
        DIR* d;
        ~dir_guard() {
            if (d != nullptr) {
                ::closedir(d);
            }
        }
    } guard {dir};

    std::uint64_t count = 0;
    for (;;) {
        errno = 0;
        struct ::dirent* ent = ::readdir(dir);
        if (ent == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (ent->d_name[0] != '\0') {
            bool digits = true;
            for (const char* p = ent->d_name; *p != '\0'; ++p) {
                if (!std::isdigit(static_cast<unsigned char>(*p))) {
                    digits = false;
                    break;
                }
            }
            if (digits) {
                ++count;
            }
        }
    }
    return count;
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
    return fail(errc::not_supported);
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
