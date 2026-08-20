#ifndef SYSCAPE_DETAIL_OS_LINUX_HPP
#define SYSCAPE_DETAIL_OS_LINUX_HPP

#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <fcntl.h>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

class file_descriptor {
public:
    explicit file_descriptor(int value) noexcept : value_(value) {}
    file_descriptor(const file_descriptor&) = delete;
    file_descriptor& operator=(const file_descriptor&) = delete;
    ~file_descriptor() { if (value_ >= 0) { ::close(value_); } }

private:
    int value_;
};

inline result<std::string> read_text_file(const char* path) {
    const int descriptor = ::open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const file_descriptor owned_descriptor(descriptor);
    static_cast<void>(owned_descriptor);

    constexpr std::size_t maximum_size = 1024U * 1024U;
    char buffer[4096];
    std::string output;
    for (;;) {
        const ssize_t count = ::read(descriptor, buffer, sizeof(buffer));
        if (count > 0) {
            const std::size_t size = static_cast<std::size_t>(count);
            if (output.size() > maximum_size - size) {
                return fail(errc::value_too_large);
            }
            output.append(buffer, size);
            continue;
        }
        if (count == 0) {
            return output;
        }
        if (errno != EINTR) {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
}

inline void trim_line_end(std::string& value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
}

struct release_information {
    std::string name;
    std::string pretty_name;
    std::string version_id;
    std::string build_id;
    std::string image_id;
};

inline bool is_space(char value) noexcept {
    return value == ' ' || value == '\t';
}

inline result<std::string> decode_os_release_value(std::string_view input) {
    while (!input.empty() && is_space(input.front())) { input.remove_prefix(1U); }
    while (!input.empty() && is_space(input.back())) { input.remove_suffix(1U); }
    if (input.empty()) { return std::string(); }

    const char quote = input.front();
    if (quote != '\'' && quote != '"') {
        return std::string(input);
    }
    if (input.size() < 2U || input.back() != quote) {
        return fail(errc::malformed_data);
    }

    std::string output;
    for (std::size_t index = 1U; index + 1U < input.size(); ++index) {
        const char value = input[index];
        if (value == '\\' && quote == '"') {
            if (index + 2U >= input.size()) {
                return fail(errc::malformed_data);
            }
            output.push_back(input[++index]);
        } else {
            output.push_back(value);
        }
    }
    return output;
}

inline result<release_information> parse_os_release(std::string_view input) {
    release_information output;
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const std::size_t end = input.find('\n', offset);
        std::string_view line = input.substr(
            offset, end == std::string_view::npos ? input.size() - offset
                                                   : end - offset);
        offset = end == std::string_view::npos ? input.size() : end + 1U;
        if (!line.empty() && line.back() == '\r') { line.remove_suffix(1U); }
        while (!line.empty() && is_space(line.front())) { line.remove_prefix(1U); }
        if (line.empty() || line.front() == '#') { continue; }

        const std::size_t separator = line.find('=');
        if (separator == std::string_view::npos || separator == 0U) {
            return fail(errc::malformed_data);
        }
        const std::string_view key = line.substr(0U, separator);
        const result<std::string> value =
            decode_os_release_value(line.substr(separator + 1U));
        if (!value) { return fail(value.error()); }

        if (key == "NAME") { output.name = *value; }
        else if (key == "PRETTY_NAME") { output.pretty_name = *value; }
        else if (key == "VERSION_ID") { output.version_id = *value; }
        else if (key == "BUILD_ID") { output.build_id = *value; }
        else if (key == "IMAGE_ID") { output.image_id = *value; }
    }
    return output;
}

inline result<release_information> read_os_release() {
    result<std::string> content = read_text_file("/etc/os-release");
    if (!content && content.error() == std::errc::no_such_file_or_directory) {
        content = read_text_file("/usr/lib/os-release");
    }
    if (!content) { return fail(content.error()); }
    return parse_os_release(*content);
}

inline result<struct utsname> uname_information() {
    struct utsname value {};
    if (::uname(&value) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return value;
}

inline result<std::string> product_name() {
    const result<release_information> release = read_os_release();
    if (!release) { return fail(release.error()); }
    if (!release->pretty_name.empty()) { return release->pretty_name; }
    if (!release->name.empty()) { return release->name; }
    return fail(errc::not_found);
}

inline result<std::string> product_version() {
    const result<release_information> release = read_os_release();
    if (!release) { return fail(release.error()); }
    return release->version_id.empty() ? result<std::string>(fail(errc::not_found))
                                       : result<std::string>(release->version_id);
}

inline result<std::string> build_identifier() {
    const result<release_information> release = read_os_release();
    if (!release) { return fail(release.error()); }
    if (!release->build_id.empty()) { return release->build_id; }
    if (!release->image_id.empty()) { return release->image_id; }
    return fail(errc::not_found);
}

inline result<std::string> kernel_name() {
    const result<struct utsname> value = uname_information();
    return value ? result<std::string>(std::string(value->sysname))
                 : result<std::string>(fail(value.error()));
}

inline result<std::string> kernel_version() {
    const result<struct utsname> value = uname_information();
    return value ? result<std::string>(std::string(value->release))
                 : result<std::string>(fail(value.error()));
}

inline result<std::string> host_name() {
    std::vector<char> buffer(256U);
    constexpr std::size_t maximum_size = 1024U * 1024U;
    while (buffer.size() <= maximum_size) {
        errno = 0;
        if (::gethostname(buffer.data(), buffer.size()) == 0) {
            std::size_t end = 0U;
            while (end < buffer.size() && buffer[end] != '\0') { ++end; }
            if (end < buffer.size()) { return std::string(buffer.data(), end); }
            buffer.resize(buffer.size() * 2U);
            continue;
        }
        if (errno != ENAMETOOLONG && errno != EINVAL) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        buffer.resize(buffer.size() * 2U);
    }
    return fail(errc::value_too_large);
}

inline result<std::string> boot_identifier() {
    result<std::string> value = read_text_file("/proc/sys/kernel/random/boot_id");
    if (!value) { return fail(value.error()); }
    trim_line_end(*value);
    return value->empty() ? result<std::string>(fail(errc::malformed_data))
                          : value;
}

inline result<std::chrono::milliseconds> uptime() {
    struct timespec value {};
#if defined(CLOCK_BOOTTIME)
    constexpr clockid_t clock = CLOCK_BOOTTIME;
#else
    constexpr clockid_t clock = CLOCK_MONOTONIC;
#endif
    if (::clock_gettime(clock, &value) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (value.tv_sec < 0 || value.tv_nsec < 0 || value.tv_nsec >= 1000000000L) {
        return fail(errc::malformed_data);
    }
    const auto maximum_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::milliseconds::max()).count();
    if (value.tv_sec > maximum_seconds) { return fail(errc::value_too_large); }
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::seconds(value.tv_sec) + std::chrono::nanoseconds(value.tv_nsec));
}

inline result<std::chrono::system_clock::time_point> boot_time() {
    const result<std::string> statistics = read_text_file("/proc/stat");
    if (!statistics) { return fail(statistics.error()); }

    std::size_t offset = 0U;
    while (offset < statistics->size()) {
        const std::size_t end = statistics->find('\n', offset);
        const std::string_view line(statistics->data() + offset,
            end == std::string::npos ? statistics->size() - offset : end - offset);
        if (line.size() > 6U && line.substr(0U, 6U) == "btime ") {
            long long seconds_since_epoch = 0;
            const char* first = line.data() + 6U;
            const char* last = line.data() + line.size();
            const std::from_chars_result parsed =
                std::from_chars(first, last, seconds_since_epoch);
            if (parsed.ec != std::errc() || parsed.ptr != last ||
                seconds_since_epoch < 0) {
                return fail(errc::malformed_data);
            }
            using clock = std::chrono::system_clock;
            const auto maximum = std::chrono::duration_cast<std::chrono::seconds>(
                clock::duration::max()).count();
            if (seconds_since_epoch > maximum) {
                return fail(errc::value_too_large);
            }
            return clock::time_point(std::chrono::duration_cast<clock::duration>(
                std::chrono::seconds(seconds_since_epoch)));
        }
        if (end == std::string::npos) { break; }
        offset = end + 1U;
    }
    return fail(errc::not_found);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
