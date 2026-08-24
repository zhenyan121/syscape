#include <iostream>
#include <syscape/audio.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_audio_backend() {
    const auto devs = syscape::audio::devices();
    if (devs) {
        for (const auto& dev : *devs) {
            expect(!dev.id.empty(), "Device id must not be empty");
            expect(!dev.name.empty(), "Device name must not be empty");
        }
    } else {
        expect(static_cast<bool>(devs.error()),
               "Failure must carry a nonzero error code");
    }

    const auto playbacks = syscape::audio::playback_devices();
    if (playbacks) {
        for (const auto& dev : *playbacks) {
            expect(dev.direction == syscape::audio::audio_device_direction::playback ||
                   dev.direction == syscape::audio::audio_device_direction::duplex,
                   "Playback device must have playback or duplex direction");
        }
    }

    const auto captures = syscape::audio::capture_devices();
    if (captures) {
        for (const auto& dev : *captures) {
            expect(dev.direction == syscape::audio::audio_device_direction::capture ||
                   dev.direction == syscape::audio::audio_device_direction::duplex,
                   "Capture device must have capture or duplex direction");
        }
    }

    const auto count = syscape::audio::device_count();
    expect(count || static_cast<bool>(count.error()),
           "device_count failure must carry an error code");

    const auto default_playback =
        syscape::audio::default_playback_device();
    if (default_playback) {
        expect(default_playback->is_default_playback.value_or(false),
               "Default playback device must be marked for playback");
    } else {
        expect(static_cast<bool>(default_playback.error()),
               "Default playback failure must carry an error code");
    }

    const auto default_capture = syscape::audio::default_capture_device();
    if (default_capture) {
        expect(default_capture->is_default_capture.value_or(false),
               "Default capture device must be marked for capture");
    } else {
        expect(static_cast<bool>(default_capture.error()),
               "Default capture failure must carry an error code");
    }
}

} // namespace

int main() {
    test_windows_audio_backend();
    return failures == 0 ? 0 : 1;
}
