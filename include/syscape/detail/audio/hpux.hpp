#ifndef SYSCAPE_DETAIL_AUDIO_HPUX_HPP
#define SYSCAPE_DETAIL_AUDIO_HPUX_HPP

#include <cstddef>
#include <vector>

#include <syscape/audio.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace audio_backend {

inline result<std::vector<::syscape::audio::audio_device>> devices() {
    return fail(errc::not_supported);
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
    return fail(errc::not_supported);
}

} // namespace audio_backend
} // namespace detail
} // namespace syscape

#endif
