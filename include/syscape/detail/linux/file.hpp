#ifndef SYSCAPE_DETAIL_LINUX_FILE_HPP
#define SYSCAPE_DETAIL_LINUX_FILE_HPP

#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <string>
#include <system_error>
#include <unistd.h>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace linux_platform {

class file_descriptor {
public:
    explicit file_descriptor(int value) noexcept : value_(value) {}
    file_descriptor(const file_descriptor&) = delete;
    file_descriptor& operator=(const file_descriptor&) = delete;
    ~file_descriptor() { if (value_ >= 0) { ::close(value_); } }

private:
    int value_;
};

inline result<std::string> read_text_file(
    const char* path, std::size_t maximum_size = 1024U * 1024U) {
    const int descriptor = ::open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const file_descriptor owned_descriptor(descriptor);
    static_cast<void>(owned_descriptor);

    char buffer[4096];
    std::string output;
    for (;;) {
        const ssize_t count = ::read(descriptor, buffer, sizeof(buffer));
        if (count > 0) {
            const std::size_t size = static_cast<std::size_t>(count);
            if (output.size() > maximum_size ||
                size > maximum_size - output.size()) {
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

} // namespace linux_platform
} // namespace detail
} // namespace syscape

#endif
