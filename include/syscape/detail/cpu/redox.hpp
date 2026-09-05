#ifndef SYSCAPE_DETAIL_CPU_REDOX_HPP
#define SYSCAPE_DETAIL_CPU_REDOX_HPP

#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

inline result<std::vector<std::string>> vendor_identifiers() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> model_names() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> parse_cpu_count(std::string_view content) {
    std::size_t pos = 0U;
    while (pos < content.size()) {
        const std::size_t end = content.find('\n', pos);
        std::string_view line = (end == std::string_view::npos)
                                    ? content.substr(pos)
                                    : content.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }
        if (line.rfind("CPUs:", 0) == 0) {
            std::string_view val = line.substr(5U);
            while (!val.empty() &&
                   (val.front() == ' ' || val.front() == '\t')) {
                val.remove_prefix(1U);
            }
            while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) {
                val.remove_suffix(1U);
            }
            if (val.empty()) {
                return fail(errc::malformed_data);
            }
            std::uint32_t count = 0U;
            for (const char c : val) {
                if (c < '0' || c > '9') {
                    return fail(errc::malformed_data);
                }
                const auto digit = static_cast<std::uint32_t>(c - '0');
                if (count >
                    ((std::numeric_limits<std::uint32_t>::max)() - digit) /
                        10U) {
                    return fail(errc::value_too_large);
                }
                count = count * 10U + digit;
            }
            if (count == 0U) {
                return fail(errc::malformed_data);
            }
            return count;
        }
        if (end == std::string_view::npos) {
            break;
        }
        pos = end + 1U;
    }
    return fail(errc::malformed_data);
}

struct system_cpu_reader {
    static result<std::string> read_cpu_file() {
        int fd = -1;
        for (;;) {
            fd = ::open("/scheme/sys/cpu", O_RDONLY | O_CLOEXEC);
            if (fd >= 0) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            const int err = errno;
            if (err == ENOENT) {
                return fail(errc::not_supported);
            }
            if (err == EACCES || err == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(err, std::generic_category()));
        }
        struct fd_guard {
            int d;
            ~fd_guard() {
                if (d >= 0) {
                    ::close(d);
                }
            }
        } guard {fd};

        std::string content;
        char buffer[128];
        for (;;) {
            const ssize_t bytes = ::read(fd, buffer, sizeof(buffer));
            if (bytes == 0) {
                break;
            }
            if (bytes < 0) {
                if (errno == EINTR) {
                    continue;
                }
                const int err = errno;
                if (err == EACCES || err == EPERM) {
                    return fail(errc::permission_denied);
                }
                return fail(std::error_code(err, std::generic_category()));
            }
            if (content.size() + static_cast<std::size_t>(bytes) > 4096U) {
                return fail(errc::value_too_large);
            }
            content.append(buffer, static_cast<std::size_t>(bytes));
        }
        return content;
    }
};

template <typename CpuReader = system_cpu_reader>
inline result<std::uint32_t> online_logical_processor_count() {
    auto content = CpuReader::read_cpu_file();
    if (!content) {
        return fail(content.error());
    }
    return parse_cpu_count(*content);
}

inline result<std::uint32_t> online_physical_core_count() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_processor_package_count() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> minimum_frequency_khz() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> maximum_frequency_khz() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    return fail(errc::not_supported);
}

inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> instruction_set_features() {
    return fail(errc::not_supported);
}

inline result<cpu_common::usage_information> cumulative_processor_usage() {
    return fail(errc::not_supported);
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
