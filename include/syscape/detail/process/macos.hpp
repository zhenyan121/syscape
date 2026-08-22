#ifndef SYSCAPE_DETAIL_PROCESS_MACOS_HPP
#define SYSCAPE_DETAIL_PROCESS_MACOS_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <crt_externs.h>
#include <mach-o/dyld.h>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> process_id_value(pid_t value,
                                              bool zero_is_valid) {
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

inline result<std::string> executable_path() {
    std::vector<char> buffer(1024U);
    std::uint32_t size = static_cast<std::uint32_t>(buffer.size());
    int status = ::_NSGetExecutablePath(buffer.data(), &size);
    if (status == -1) {
        if (size == 0U) { return fail(errc::malformed_data); }
        buffer.resize(size);
        size = static_cast<std::uint32_t>(buffer.size());
        status = ::_NSGetExecutablePath(buffer.data(), &size);
    }
    if (status != 0) { return fail(errc::io_error); }

    std::size_t length = 0U;
    while (length < buffer.size() && buffer[length] != '\0') { ++length; }
    if (length >= buffer.size()) { return fail(errc::malformed_data); }
    const std::string path(buffer.data(), length);
    return path.empty() || path.front() != '/'
        ? result<std::string>(fail(errc::malformed_data))
        : result<std::string>(path);
}

inline result<std::vector<std::string>> command_line() {
    const int* count_pointer = ::_NSGetArgc();
    char*** argument_storage = ::_NSGetArgv();
    if (count_pointer == nullptr || argument_storage == nullptr ||
        *argument_storage == nullptr) {
        return fail(errc::not_found);
    }

    const int count = *count_pointer;
    if (count < 0) { return fail(errc::malformed_data); }
    std::vector<std::string> values;
    if (static_cast<std::size_t>(count) > values.max_size()) {
        return fail(errc::resource_exhausted);
    }
    values.resize(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        const char* argument = (*argument_storage)[index];
        if (argument == nullptr) { return fail(errc::malformed_data); }
        values[static_cast<std::size_t>(index)] = std::string(argument);
    }
    return values;
}

inline result<std::string> working_directory() {
    std::vector<char> buffer(1024U);
    constexpr std::size_t maximum_size = 1024U * 1024U;
    for (;;) {
        errno = 0;
        const char* value = ::getcwd(buffer.data(), buffer.size());
        if (value != nullptr) {
            const std::string path(buffer.data());
            return path.empty() || path.front() != '/'
                ? result<std::string>(fail(errc::malformed_data))
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
