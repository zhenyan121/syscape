#ifndef SYSCAPE_DETAIL_LINUX_DIRECTORY_HPP
#define SYSCAPE_DETAIL_LINUX_DIRECTORY_HPP

#include <dirent.h>

namespace syscape {
namespace detail {
namespace linux_platform {

/// Owns one open directory stream for the duration of an enumeration.
class directory_handle {
public:
    explicit directory_handle(const char* path) noexcept
        : value_(::opendir(path)) {}
    directory_handle(const directory_handle&) = delete;
    directory_handle& operator=(const directory_handle&) = delete;
    ~directory_handle() {
        if (value_ != nullptr) { ::closedir(value_); }
    }

    /// Returns true when the directory opened successfully.
    bool valid() const noexcept { return value_ != nullptr; }
    /// Returns the owned stream.
    ::DIR* get() const noexcept { return value_; }

private:
    ::DIR* value_;
};

} // namespace linux_platform
} // namespace detail
} // namespace syscape

#endif
