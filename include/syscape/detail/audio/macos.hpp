#ifndef SYSCAPE_DETAIL_AUDIO_MACOS_HPP
#define SYSCAPE_DETAIL_AUDIO_MACOS_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <syscape/audio.hpp>
#include <syscape/detail/audio/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace audio_backend {

class osstatus_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "macos-osstatus"; }

    std::string message(int value) const override {
        return "macOS OSStatus " + std::to_string(value);
    }
};

inline const std::error_category& osstatus_category() noexcept {
    static const osstatus_error_category category;
    return category;
}

inline std::error_code map_osstatus(OSStatus value) noexcept {
    return std::error_code(static_cast<int>(value), osstatus_category());
}

class cf_object {
public:
    explicit cf_object(::CFTypeRef value) noexcept : value_(value) {}
    cf_object(const cf_object&) = delete;
    cf_object& operator=(const cf_object&) = delete;
    ~cf_object() {
        if (value_ != nullptr) {
            ::CFRelease(value_);
        }
    }
    ::CFTypeRef get() const noexcept { return value_; }

private:
    ::CFTypeRef value_;
};

inline result<std::string> copy_utf8_string(::CFStringRef value) {
    if (value == nullptr) {
        return fail(errc::io_error);
    }
    const ::CFIndex length = ::CFStringGetLength(value);
    const ::CFIndex maximum = ::CFStringGetMaximumSizeForEncoding(
        length, ::kCFStringEncodingUTF8);
    if (maximum < 0) {
        return fail(errc::io_error);
    }
    if (maximum == std::numeric_limits<::CFIndex>::max() ||
        static_cast<unsigned long long>(maximum) >=
            static_cast<unsigned long long>(
                std::numeric_limits<std::size_t>::max())) {
        return fail(errc::value_too_large);
    }
    std::string output;
    output.resize(static_cast<std::size_t>(maximum) + 1U);
    if (!::CFStringGetCString(value, output.data(), maximum + 1,
                              ::kCFStringEncodingUTF8)) {
        return fail(errc::invalid_encoding);
    }
    output.resize(std::char_traits<char>::length(output.c_str()));
    if (!is_valid_utf8(output)) {
        return fail(errc::invalid_encoding);
    }
    return output;
}

inline result<std::uint32_t> count_channels_for_scope(
    ::AudioDeviceID dev_id, ::AudioObjectPropertyScope scope) {
    ::AudioObjectPropertyAddress addr;
    addr.mSelector = kAudioDevicePropertyStreamConfiguration;
    addr.mScope = scope;
    addr.mElement = kAudioObjectPropertyElementMain;

    UInt32 data_size = 0U;
    OSStatus status = ::AudioObjectGetPropertyDataSize(
        dev_id, &addr, 0, nullptr, &data_size);
    if (status != noErr) {
        return fail(map_osstatus(status));
    }
    if (data_size == 0U) {
        return 0U;
    }

    const std::size_t unit_size = sizeof(std::max_align_t);
    const std::size_t unit_count =
        (static_cast<std::size_t>(data_size) + unit_size - 1U) / unit_size;
    std::vector<std::max_align_t> buffer(unit_count);
    auto* buffer_list = reinterpret_cast<AudioBufferList*>(buffer.data());
    status = ::AudioObjectGetPropertyData(
        dev_id, &addr, 0, nullptr, &data_size, buffer_list);
    if (status != noErr) {
        return fail(map_osstatus(status));
    }
    const std::size_t buffer_offset = offsetof(AudioBufferList, mBuffers);
    if (data_size < buffer_offset ||
        buffer_list->mNumberBuffers >
            (static_cast<std::size_t>(data_size) - buffer_offset) /
                sizeof(AudioBuffer)) {
        return fail(errc::malformed_data);
    }

    std::uint32_t total_channels = 0U;
    for (UInt32 i = 0U; i < buffer_list->mNumberBuffers; ++i) {
        const std::uint32_t channels =
            buffer_list->mBuffers[i].mNumberChannels;
        if (channels > std::numeric_limits<std::uint32_t>::max() -
                           total_channels) {
            return fail(errc::value_too_large);
        }
        total_channels += channels;
    }
    return total_channels;
}

inline result<::AudioDeviceID> default_device_id(
    ::AudioObjectPropertySelector selector) {
    ::AudioObjectPropertyAddress addr;
    addr.mSelector = selector;
    addr.mScope = kAudioObjectPropertyScopeGlobal;
    addr.mElement = kAudioObjectPropertyElementMain;
    ::AudioDeviceID id = kAudioObjectUnknown;
    UInt32 size = sizeof(id);
    const OSStatus status = ::AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &addr, 0, nullptr, &size, &id);
    if (status != noErr) {
        return fail(map_osstatus(status));
    }
    if (id == kAudioObjectUnknown) {
        return fail(errc::not_found);
    }
    return id;
}

inline result<std::vector<::syscape::audio::audio_device>> collect_devices(
    std::optional<::AudioObjectPropertySelector> required_default = std::nullopt) {
    ::AudioObjectPropertyAddress addr;
    addr.mSelector = kAudioHardwarePropertyDevices;
    addr.mScope = kAudioObjectPropertyScopeGlobal;
    addr.mElement = kAudioObjectPropertyElementMain;

    UInt32 data_size = 0U;
    OSStatus status = ::AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject, &addr, 0, nullptr, &data_size);
    if (status != noErr) {
        return fail(map_osstatus(status));
    }

    if (data_size % sizeof(::AudioDeviceID) != 0U) {
        return fail(errc::malformed_data);
    }

    const std::size_t count = data_size / sizeof(::AudioDeviceID);
    std::vector<::AudioDeviceID> dev_ids(count);
    status = ::AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &addr, 0, nullptr, &data_size, dev_ids.data());
    if (status != noErr) {
        return fail(map_osstatus(status));
    }

    const auto default_out_id =
        default_device_id(kAudioHardwarePropertyDefaultOutputDevice);
    const auto default_in_id =
        default_device_id(kAudioHardwarePropertyDefaultInputDevice);
    if (required_default == kAudioHardwarePropertyDefaultOutputDevice &&
        !default_out_id) {
        return fail(default_out_id.error());
    }
    if (required_default == kAudioHardwarePropertyDefaultInputDevice &&
        !default_in_id) {
        return fail(default_in_id.error());
    }

    std::vector<::syscape::audio::audio_device> devices;
    devices.reserve(count);

    for (::AudioDeviceID dev_id : dev_ids) {
        ::syscape::audio::audio_device audio_dev;
        audio_dev.state = ::syscape::audio::audio_device_state::unknown;

        // Device UID
        addr.mSelector = kAudioDevicePropertyDeviceUID;
        addr.mScope = kAudioObjectPropertyScopeGlobal;
        addr.mElement = kAudioObjectPropertyElementMain;
        ::CFStringRef uid_ref = nullptr;
        UInt32 ref_size = sizeof(::CFStringRef);
        status = ::AudioObjectGetPropertyData(
            dev_id, &addr, 0, nullptr, &ref_size, &uid_ref);
        if (status != noErr) {
            return fail(map_osstatus(status));
        }
        if (uid_ref == nullptr) {
            return fail(errc::malformed_data);
        }
        cf_object uid_guard(uid_ref);
        const auto uid_str = copy_utf8_string(uid_ref);
        if (!uid_str) {
            return fail(uid_str.error());
        }
        audio_dev.id = *uid_str;

        // Device Name
        addr.mSelector = kAudioDevicePropertyDeviceNameCFString;
        ::CFStringRef name_ref = nullptr;
        ref_size = sizeof(::CFStringRef);
        status = ::AudioObjectGetPropertyData(
            dev_id, &addr, 0, nullptr, &ref_size, &name_ref);
        if (status != noErr) {
            return fail(map_osstatus(status));
        }
        if (name_ref == nullptr) {
            return fail(errc::malformed_data);
        }
        cf_object name_guard(name_ref);
        const auto name_str = copy_utf8_string(name_ref);
        if (!name_str) {
            return fail(name_str.error());
        }
        audio_dev.name = *name_str;

        // Channels & Direction
        const auto out_channels = count_channels_for_scope(
            dev_id, kAudioObjectPropertyScopeOutput);
        const auto in_channels = count_channels_for_scope(
            dev_id, kAudioObjectPropertyScopeInput);

        if (!out_channels || !in_channels) {
            return fail(!out_channels ? out_channels.error()
                                      : in_channels.error());
        }

        if (*out_channels > 0U && *in_channels > 0U) {
            audio_dev.direction = ::syscape::audio::audio_device_direction::duplex;
            audio_dev.playback_channels = *out_channels;
            audio_dev.capture_channels = *in_channels;
        } else if (*out_channels > 0U) {
            audio_dev.direction = ::syscape::audio::audio_device_direction::playback;
            audio_dev.playback_channels = *out_channels;
        } else if (*in_channels > 0U) {
            audio_dev.direction = ::syscape::audio::audio_device_direction::capture;
            audio_dev.capture_channels = *in_channels;
        } else {
            audio_dev.direction = ::syscape::audio::audio_device_direction::unknown;
        }

        // Sample rate
        addr.mSelector = kAudioDevicePropertyNominalSampleRate;
        Float64 sample_rate = 0.0;
        UInt32 rate_size = sizeof(Float64);
        status = ::AudioObjectGetPropertyData(
            dev_id, &addr, 0, nullptr, &rate_size, &sample_rate);
        if (status == noErr) {
            if (!(sample_rate > 0.0) ||
                sample_rate > static_cast<Float64>(
                                  std::numeric_limits<std::uint32_t>::max())) {
                return fail(errc::malformed_data);
            }
            audio_dev.sample_rate_hz =
                static_cast<std::uint32_t>(sample_rate);
        }

        addr.mSelector = kAudioDevicePropertyDeviceIsAlive;
        UInt32 alive = 0U;
        UInt32 alive_size = sizeof(alive);
        if (::AudioObjectGetPropertyData(dev_id, &addr, 0, nullptr,
                                         &alive_size, &alive) == noErr) {
            audio_dev.state = alive != 0U
                                  ? ::syscape::audio::audio_device_state::active
                                  : ::syscape::audio::audio_device_state::not_present;
        }

        if (default_out_id) {
            audio_dev.is_default_playback = dev_id == *default_out_id;
        }
        if (default_in_id) {
            audio_dev.is_default_capture = dev_id == *default_in_id;
        }

        devices.push_back(std::move(audio_dev));
    }

    return devices;
}

inline result<std::vector<::syscape::audio::audio_device>> devices() {
    return collect_devices();
}

inline result<std::vector<::syscape::audio::audio_device>> playback_devices() {
    const auto all = collect_devices();
    if (!all) {
        return fail(all.error());
    }
    return audio_common::filter_by_direction(*all, ::syscape::audio::audio_device_direction::playback);
}

inline result<std::vector<::syscape::audio::audio_device>> capture_devices() {
    const auto all = collect_devices();
    if (!all) {
        return fail(all.error());
    }
    return audio_common::filter_by_direction(*all, ::syscape::audio::audio_device_direction::capture);
}

inline result<::syscape::audio::audio_device> default_playback_device() {
    const auto all = collect_devices(kAudioHardwarePropertyDefaultOutputDevice);
    if (!all) {
        return fail(all.error());
    }
    return audio_common::find_default_device(*all, ::syscape::audio::audio_device_direction::playback);
}

inline result<::syscape::audio::audio_device> default_capture_device() {
    const auto all = collect_devices(kAudioHardwarePropertyDefaultInputDevice);
    if (!all) {
        return fail(all.error());
    }
    return audio_common::find_default_device(*all, ::syscape::audio::audio_device_direction::capture);
}

inline result<std::size_t> device_count() {
    const auto all = collect_devices();
    if (!all) {
        return fail(all.error());
    }
    return all->size();
}

} // namespace audio_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_AUDIO_MACOS_HPP
