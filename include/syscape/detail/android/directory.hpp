#ifndef SYSCAPE_DETAIL_ANDROID_DIRECTORY_HPP
#define SYSCAPE_DETAIL_ANDROID_DIRECTORY_HPP

#include <cerrno>
#include <dirent.h>

namespace syscape {
namespace detail {
namespace android {

/// Owns one open directory stream for the duration of an enumeration.
class directory_handle {
    public:
    explicit directory_handle(const char* path) noexcept
        : value_(::opendir(path)), error_(value_ == nullptr ? errno : 0) {}
    directory_handle(const directory_handle&) = delete;
    directory_handle& operator=(const directory_handle&) = delete;
    ~directory_handle() {
        if (value_ != nullptr) {
            ::closedir(value_);
        }
    }

    /// Returns true when the directory opened successfully.
    bool valid() const noexcept {
        return value_ != nullptr;
    }
    /// Returns the errno value captured when opening failed, or zero.
    int error() const noexcept {
        return error_;
    }
    /// Returns the owned stream.
    ::DIR* get() const noexcept {
        return value_;
    }

    private:
    ::DIR* value_;
    int error_;
};

} // namespace android
} // namespace detail
} // namespace syscape

#endif
