#ifndef SYSCAPE_DETAIL_PROCESS_LINUX_HPP
#define SYSCAPE_DETAIL_PROCESS_LINUX_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/linux/file.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> process_id_value(
    pid_t value, bool zero_is_valid) {
    if (value < 0 || (!zero_is_valid && value == 0)) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(value);
}

inline result<std::uint32_t> process_id() {
    return process_id_value(::getpid(), false);
}

inline result<std::uint32_t> parent_process_id() {
    return process_id_value(::getppid(), true);
}

inline result<std::vector<std::string>> parse_command_line(
    std::string_view input) {
    if (input.empty()) { return std::vector<std::string>(); }
    if (input.back() != '\0') { return fail(errc::malformed_data); }
    input.remove_suffix(1U);

    std::vector<std::string> arguments;
    std::size_t start = 0U;
    for (;;) {
        const std::size_t end = input.find('\0', start);
        if (end == std::string_view::npos) {
            arguments.emplace_back(input.substr(start));
            break;
        }
        arguments.emplace_back(input.substr(start, end - start));
        start = end + 1U;
    }
    return arguments;
}

template <typename ReadLinkOperation>
inline result<std::string> read_link_with_growth(
    ReadLinkOperation read_link) {
    constexpr std::size_t initial_size = 256U;
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(initial_size);

    for (;;) {
        errno = 0;
        const ssize_t count =
            read_link(buffer.data(), buffer.size());
        if (count >= 0 && static_cast<std::size_t>(count) < buffer.size()) {
            return std::string(buffer.data(), static_cast<std::size_t>(count));
        }
        if (count < 0) {
            if (errno != EINTR) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            continue;
        }
        if (buffer.size() > maximum_size / 2U) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() * 2U);
    }
}

inline result<std::string> executable_path() {
    const result<std::string> value = read_link_with_growth(
        [](char* buffer, std::size_t size) {
            return ::readlink("/proc/self/exe", buffer, size);
        });
    if (!value || value->empty() || value->front() != '/') {
        return value ? result<std::string>(fail(errc::malformed_data))
                     : result<std::string>(fail(value.error()));
    }
    return value;
}

inline result<std::vector<std::string>> command_line() {
    result<std::string> input =
        linux_platform::read_text_file("/proc/self/cmdline");
    if (!input) { return fail(input.error()); }
    return parse_command_line(*input);
}

inline result<std::string> working_directory() {
    std::vector<char> buffer(1024U);
    constexpr std::size_t maximum_size = 1024U * 1024U;
    for (;;) {
        errno = 0;
        const char* value = ::getcwd(buffer.data(), buffer.size());
        if (value != nullptr) {
            const std::string path(buffer.data());
            return path.empty() ? result<std::string>(fail(errc::malformed_data))
                                : result<std::string>(std::move(path));
        }
        if (errno != ERANGE) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (buffer.size() >= maximum_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() <= maximum_size / 2U
                          ? buffer.size() * 2U
                          : maximum_size);
    }
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
