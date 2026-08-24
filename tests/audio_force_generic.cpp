#include <cassert>
#include <syscape/audio.hpp>

int main() {
    const auto devs = syscape::audio::devices();
    assert(!devs);
    assert(devs.error() == syscape::errc::not_supported);

    const auto playbacks = syscape::audio::playback_devices();
    assert(!playbacks);
    assert(playbacks.error() == syscape::errc::not_supported);

    const auto captures = syscape::audio::capture_devices();
    assert(!captures);
    assert(captures.error() == syscape::errc::not_supported);

    const auto def_play = syscape::audio::default_playback_device();
    assert(!def_play);
    assert(def_play.error() == syscape::errc::not_supported);

    const auto def_cap = syscape::audio::default_capture_device();
    assert(!def_cap);
    assert(def_cap.error() == syscape::errc::not_supported);

    const auto count = syscape::audio::device_count();
    assert(!count);
    assert(count.error() == syscape::errc::not_supported);

    return 0;
}
