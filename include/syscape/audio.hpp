#ifndef SYSCAPE_AUDIO_HPP
#define SYSCAPE_AUDIO_HPP

/// @file
/// @brief Hosted audio devices, playback/capture endpoints, capabilities, and default devices.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note This module exposes:
/// - Enumeration of audio endpoints/devices (devices(), playback_devices(), capture_devices()).
/// - Identification of default playback and capture devices (default_playback_device(), default_capture_device()).
/// - Total audio device count (device_count()).
/// - Stream direction (playback, capture, duplex).
/// - Channel counts, sample rates, sound card and driver associations.
/// @note Linux queries ALSA kernel interfaces (/proc/asound/cards, /proc/asound/pcm, /sys/class/sound).
/// @note Windows queries Win32 Core Audio MMDevice interfaces.
/// @note macOS queries Darwin CoreAudio HAL interfaces.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/audio.hpp requires C++17 or later"
#endif

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace syscape {
namespace audio {

/// Audio endpoint stream direction (playback, capture, duplex).
enum class audio_device_direction : std::uint8_t {
    /// Direction is unknown or unspecified.
    unknown,
    /// Audio playback / rendering (output: speakers, headphones, HDMI, SPDIF).
    playback,
    /// Audio capture / recording (input: microphone, line-in).
    capture,
    /// Bidirectional / duplex audio device (both playback and capture).
    duplex
};

/// Operational / connection state of an audio device.
enum class audio_device_state : std::uint8_t {
    /// Device state is unknown or unspecified.
    unknown,
    /// Device is active, connected, and available for use.
    active,
    /// Device is disabled by the user or system configuration.
    disabled,
    /// Device endpoint is unplugged (e.g. 3.5mm jack not connected).
    unplugged,
    /// Device is not present or hardware removed.
    not_present
};

/// Information describing a single audio input or output endpoint/device.
struct audio_device {
    /// Platform-specific identifier or path (e.g. "hw:0,0", MMDevice Endpoint ID, CoreAudio UID).
    std::string id;

    /// Human-readable device or endpoint name (e.g. "ALC287 Analog", "Realtek High Definition Audio", "MacBook Pro Speakers").
    std::string name;

    /// Stream direction (playback, capture, or duplex).
    audio_device_direction direction = audio_device_direction::unknown;

    /// Operational / connection state, or unknown when the platform does not
    /// expose an authoritative state for this endpoint.
    audio_device_state state = audio_device_state::unknown;

    /// Underlying sound card or audio controller name, if exposed.
    std::optional<std::string> card_name;

    /// Kernel/OS driver or module name (e.g. "HDA-Intel", "snd_hda_intel"), if exposed.
    std::optional<std::string> driver_name;

    /// Supported or active playback channel count, if exposed.
    std::optional<std::uint32_t> playback_channels;

    /// Supported or active capture channel count, if exposed.
    std::optional<std::uint32_t> capture_channels;

    /// Current or nominal sample rate in Hertz (e.g. 44100, 48000, 96000), if exposed.
    std::optional<std::uint32_t> sample_rate_hz;

    /// Minimum supported sample rate in Hertz, if exposed.
    std::optional<std::uint32_t> min_sample_rate_hz;

    /// Maximum supported sample rate in Hertz, if exposed.
    std::optional<std::uint32_t> max_sample_rate_hz;

    /// Indicates whether this device is the current default playback endpoint,
    /// or disengaged when authoritative default information is unavailable.
    std::optional<bool> is_default_playback;

    /// Indicates whether this device is the current default capture endpoint,
    /// or disengaged when authoritative default information is unavailable.
    std::optional<bool> is_default_capture;
};

} // namespace audio
} // namespace syscape

#include <syscape/detail/audio/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/audio/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/audio/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/audio/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/audio/freebsd.hpp>
#else
#include <syscape/detail/audio/generic.hpp>
#endif

namespace syscape {
namespace audio {

/// Enumerates all detected audio devices/endpoints across all stream directions.
///
/// @return A vector of audio_device entries; not_supported when audio subsystem
/// is unavailable; permission_denied when access is denied; malformed_data for
/// invalid platform data; or a native I/O error.
/// @note The collection can change when devices are connected, removed, or
/// reconfigured. Linux reports ALSA kernel PCM endpoints rather than desktop
/// sound-server virtual endpoints.
inline result<std::vector<audio_device>> devices() {
    return detail::audio_backend::devices();
}

/// Enumerates all detected audio playback (output) devices/endpoints.
///
/// @return A vector of playback audio_device entries.
inline result<std::vector<audio_device>> playback_devices() {
    return detail::audio_backend::playback_devices();
}

/// Enumerates all detected audio capture (input) devices/endpoints.
///
/// @return A vector of capture audio_device entries.
inline result<std::vector<audio_device>> capture_devices() {
    return detail::audio_backend::capture_devices();
}

/// Queries the current default system audio playback (output) device.
///
/// @return The default playback audio_device; not_found if no playback device exists;
/// or not_supported if default device lookup is not supported on the platform.
/// Linux ALSA procfs does not expose an authoritative session default, so this
/// query returns not_supported there.
inline result<audio_device> default_playback_device() {
    return detail::audio_backend::default_playback_device();
}

/// Queries the current default system audio capture (input) device.
///
/// @return The default capture audio_device; not_found if no capture device exists;
/// or not_supported if default device lookup is not supported on the platform.
/// Linux ALSA procfs does not expose an authoritative session default, so this
/// query returns not_supported there.
inline result<audio_device> default_capture_device() {
    return detail::audio_backend::default_capture_device();
}

/// Returns the total count of detected audio devices.
///
/// @return Number of detected audio devices.
inline result<std::size_t> device_count() {
    return detail::audio_backend::device_count();
}

} // namespace audio
} // namespace syscape

#endif // SYSCAPE_AUDIO_HPP
