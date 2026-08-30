#ifndef SYSCAPE_DETAIL_AUDIO_FREEBSD_HPP
#define SYSCAPE_DETAIL_AUDIO_FREEBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/audio.hpp>
#include <syscape/detail/audio/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace audio_backend {

inline result<int> read_sysctl_int(const char* name) {
    int value = 0;
    std::size_t size = sizeof(value);
    if (::sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    return value;
}

inline result<std::string> read_sysctl_string(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return fail(errc::not_found);
    }
    std::string value(size, '\0');
    if (::sysctlbyname(name, &value[0], &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n' ||
                              value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

inline result<std::vector<::syscape::audio::audio_device>> devices() {
    std::vector<::syscape::audio::audio_device> list;
    const result<int> default_unit = read_sysctl_int("hw.snd.default_unit");

    for (int i = 0; i < 64; ++i) {
        const std::string unit_str = std::to_string(i);
        const std::string desc_name = "dev.pcm." + unit_str + ".%desc";
        const result<std::string> desc = read_sysctl_string(desc_name.c_str());
        if (!desc) {
            if (desc.error() != errc::not_found) {
                // If permission denied or other system error on first check,
                // fail
                if (list.empty() && i == 0) {
                    return fail(desc.error());
                }
            }
            continue;
        }

        ::syscape::audio::audio_device dev;
        dev.id = "dsp" + unit_str;
        dev.name = !desc->empty() && is_valid_utf8(*desc)
                       ? *desc
                       : "PCM Audio " + unit_str;
        dev.state = ::syscape::audio::audio_device_state::active;

        const std::string drv_name = "dev.pcm." + unit_str + ".%driver";
        const result<std::string> driver = read_sysctl_string(drv_name.c_str());
        if (driver && !driver->empty() && is_valid_utf8(*driver)) {
            dev.driver_name = *driver;
        }

        const std::string parent_name = "dev.pcm." + unit_str + ".%parent";
        const result<std::string> parent =
            read_sysctl_string(parent_name.c_str());
        if (parent && !parent->empty() && is_valid_utf8(*parent)) {
            dev.card_name = *parent;
        }

        const std::string play_chans_name =
            "dev.pcm." + unit_str + ".play.vchans";
        const result<int> play_chans = read_sysctl_int(play_chans_name.c_str());

        const std::string rec_chans_name =
            "dev.pcm." + unit_str + ".rec.vchans";
        const result<int> rec_chans = read_sysctl_int(rec_chans_name.c_str());

        const bool has_play = play_chans && *play_chans > 0;
        const bool has_rec = rec_chans && *rec_chans > 0;

        if (has_play && has_rec) {
            dev.direction = ::syscape::audio::audio_device_direction::duplex;
        } else if (has_play) {
            dev.direction = ::syscape::audio::audio_device_direction::playback;
        } else if (has_rec) {
            dev.direction = ::syscape::audio::audio_device_direction::capture;
        } else {
            dev.direction = ::syscape::audio::audio_device_direction::duplex;
        }

        if (has_play) {
            dev.playback_channels = static_cast<std::uint32_t>(*play_chans);
        }
        if (has_rec) {
            dev.capture_channels = static_cast<std::uint32_t>(*rec_chans);
        }

        const std::string rate_name = "dev.pcm." + unit_str + ".play.vchanrate";
        result<int> rate = read_sysctl_int(rate_name.c_str());
        if (!rate || *rate <= 0) {
            const std::string feeder_name =
                "hw.snd.unit" + unit_str + ".feeder_rate";
            rate = read_sysctl_int(feeder_name.c_str());
        }
        if (rate && *rate > 0) {
            dev.sample_rate_hz = static_cast<std::uint32_t>(*rate);
        }

        if (default_unit && *default_unit == i) {
            if (dev.direction ==
                    ::syscape::audio::audio_device_direction::playback ||
                dev.direction ==
                    ::syscape::audio::audio_device_direction::duplex) {
                dev.is_default_playback = true;
            }
            if (dev.direction ==
                    ::syscape::audio::audio_device_direction::capture ||
                dev.direction ==
                    ::syscape::audio::audio_device_direction::duplex) {
                dev.is_default_capture = true;
            }
        }

        list.push_back(std::move(dev));
    }

    return list;
}

inline result<std::vector<::syscape::audio::audio_device>> playback_devices() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    std::vector<::syscape::audio::audio_device> play;
    for (const auto& d : *all) {
        if (d.direction == ::syscape::audio::audio_device_direction::playback ||
            d.direction == ::syscape::audio::audio_device_direction::duplex) {
            play.push_back(d);
        }
    }
    return play;
}

inline result<std::vector<::syscape::audio::audio_device>> capture_devices() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    std::vector<::syscape::audio::audio_device> cap;
    for (const auto& d : *all) {
        if (d.direction == ::syscape::audio::audio_device_direction::capture ||
            d.direction == ::syscape::audio::audio_device_direction::duplex) {
            cap.push_back(d);
        }
    }
    return cap;
}

inline result<::syscape::audio::audio_device> default_playback_device() {
    const auto all = playback_devices();
    if (!all) {
        return fail(all.error());
    }
    for (const auto& d : *all) {
        if (d.is_default_playback && *d.is_default_playback) {
            return d;
        }
    }
    return fail(errc::not_found);
}

inline result<::syscape::audio::audio_device> default_capture_device() {
    const auto all = capture_devices();
    if (!all) {
        return fail(all.error());
    }
    for (const auto& d : *all) {
        if (d.is_default_capture && *d.is_default_capture) {
            return d;
        }
    }
    return fail(errc::not_found);
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
