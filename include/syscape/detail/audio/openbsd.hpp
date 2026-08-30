#ifndef SYSCAPE_DETAIL_AUDIO_OPENBSD_HPP
#define SYSCAPE_DETAIL_AUDIO_OPENBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/audioio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/audio.hpp>
#include <syscape/detail/audio/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace audio_backend {

inline result<std::vector<::syscape::audio::audio_device>> devices() {
    std::vector<::syscape::audio::audio_device> list;
    for (int i = 0;; ++i) {
        const std::string path = "/dev/audio" + std::to_string(i);
        const std::string ctl_path = "/dev/audioctl" + std::to_string(i);

        struct ::stat st {};
        if (::stat(path.c_str(), &st) != 0) {
            if (errno == ENOENT) {
                break;
            }
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }

        int fd = ::open(ctl_path.c_str(), O_RDONLY);
        if (fd < 0) {
            const int ctl_errno = errno;
            fd = ::open(path.c_str(), O_RDONLY);
            if (fd < 0) {
                const int path_errno = errno;
                if (ctl_errno == EACCES || ctl_errno == EPERM ||
                    path_errno == EACCES || path_errno == EPERM) {
                    return fail(errc::permission_denied);
                }
                if (path_errno == ENXIO || path_errno == ENODEV ||
                    path_errno == ENOENT) {
                    continue;
                }
                return fail(
                    std::error_code(path_errno != 0 ? path_errno : ctl_errno,
                                    std::generic_category()));
            }
        }

        struct audio_device adev {};
        if (::ioctl(fd, AUDIO_GETDEV, &adev) != 0) {
            const int ioctl_errno = errno;
            ::close(fd);
            if (ioctl_errno == ENODEV || ioctl_errno == ENXIO) {
                continue;
            }
            if (ioctl_errno == ENOTTY || ioctl_errno == EOPNOTSUPP) {
                return fail(errc::not_supported);
            }
            if (ioctl_errno == EACCES || ioctl_errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(ioctl_errno, std::generic_category()));
        }
        ::close(fd);

        if (adev.name[0] == '\0') {
            return fail(errc::malformed_data);
        }
        std::string dname(adev.name);
        if (!is_valid_utf8(dname)) {
            return fail(errc::invalid_encoding);
        }

        ::syscape::audio::audio_device dev;
        dev.id = "audio" + std::to_string(i);
        dev.name = std::move(dname);
        dev.direction = ::syscape::audio::audio_device_direction::unknown;
        dev.state = ::syscape::audio::audio_device_state::unknown;

        if (adev.version[0] != '\0') {
            std::string dver(adev.version);
            if (!is_valid_utf8(dver)) {
                return fail(errc::invalid_encoding);
            }
            dev.driver_name = std::move(dver);
        }

        list.push_back(std::move(dev));
    }
    return list;
}

inline result<std::vector<::syscape::audio::audio_device>> playback_devices() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::audio::audio_device>> capture_devices() {
    return fail(errc::not_supported);
}

inline result<::syscape::audio::audio_device> default_playback_device() {
    return fail(errc::not_supported);
}

inline result<::syscape::audio::audio_device> default_capture_device() {
    return fail(errc::not_supported);
}

inline result<std::size_t> device_count() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return all->size();
}

} // namespace audio_backend
} // namespace detail
} // namespace syscape

#endif
