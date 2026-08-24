#ifndef SYSCAPE_DETAIL_AUDIO_LINUX_HPP
#define SYSCAPE_DETAIL_AUDIO_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <syscape/audio.hpp>
#include <syscape/detail/audio/common.hpp>
#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace audio_backend {

struct alsa_card_info {
    std::uint32_t index = 0U;
    std::string id;
    std::string driver;
    std::string short_name;
    std::string long_name;
};

struct alsa_pcm_info {
    std::uint32_t card_index = 0U;
    std::uint32_t device_index = 0U;
    std::string id;
    std::string name;
    bool has_playback = false;
    bool has_capture = false;
};

/// Parses /proc/asound/cards content.
inline result<std::vector<alsa_card_info>> parse_proc_asound_cards(
    std::string_view content) {
    std::vector<alsa_card_info> cards;
    std::size_t offset = 0U;

    while (offset < content.size()) {
        const std::size_t line_end = content.find('\n', offset);
        const std::string_view line = (line_end == std::string_view::npos)
                                          ? content.substr(offset)
                                          : content.substr(offset, line_end - offset);
        offset = (line_end == std::string_view::npos) ? content.size() : line_end + 1U;

        const std::string_view trimmed = audio_common::trim_whitespace(line);
        if (trimmed.empty() || trimmed.rfind("---", 0) == 0) {
            continue;
        }

        // Card header lines start with card index: e.g. " 0 [NVidia         ]: HDA-Intel - HDA NVidia"
        if (std::isdigit(static_cast<unsigned char>(trimmed.front()))) {
            const std::size_t bracket_open = trimmed.find('[');
            const std::size_t bracket_close = trimmed.find(']');
            if (bracket_open == std::string_view::npos ||
                bracket_close == std::string_view::npos ||
                bracket_close <= bracket_open) {
                return fail(errc::malformed_data);
            }

            const std::string_view index_str =
                audio_common::trim_whitespace(trimmed.substr(0, bracket_open));
            const auto card_index = audio_common::parse_u32(index_str);
            if (!card_index) {
                return fail(card_index.error());
            }

            const std::string_view id_str =
                audio_common::trim_whitespace(trimmed.substr(bracket_open + 1U, bracket_close - bracket_open - 1U));

            alsa_card_info card;
            card.index = *card_index;
            card.id = std::string(id_str);

            const std::size_t colon_pos = trimmed.find(':', bracket_close);
            if (colon_pos != std::string_view::npos) {
                const std::string_view after_colon =
                    audio_common::trim_whitespace(trimmed.substr(colon_pos + 1U));
                const std::size_t dash_pos = after_colon.find(" - ");
                if (dash_pos != std::string_view::npos) {
                    card.driver = std::string(
                        audio_common::trim_whitespace(after_colon.substr(0, dash_pos)));
                    card.short_name = std::string(
                        audio_common::trim_whitespace(after_colon.substr(dash_pos + 3U)));
                } else {
                    card.short_name = std::string(after_colon);
                }
            }

            if (!is_valid_utf8(card.id) || !is_valid_utf8(card.driver) ||
                !is_valid_utf8(card.short_name)) {
                return fail(errc::invalid_encoding);
            }

            cards.push_back(std::move(card));
        } else if (!cards.empty()) {
            // Continuation line usually has the long name
            if (cards.back().long_name.empty()) {
                cards.back().long_name = std::string(trimmed);
                if (!is_valid_utf8(cards.back().long_name)) {
                    return fail(errc::invalid_encoding);
                }
            }
        }
    }

    return cards;
}

/// Parses /proc/asound/pcm content.
inline result<std::vector<alsa_pcm_info>> parse_proc_asound_pcm(
    std::string_view content) {
    std::vector<alsa_pcm_info> pcms;
    std::size_t offset = 0U;

    while (offset < content.size()) {
        const std::size_t line_end = content.find('\n', offset);
        const std::string_view line = (line_end == std::string_view::npos)
                                          ? content.substr(offset)
                                          : content.substr(offset, line_end - offset);
        offset = (line_end == std::string_view::npos) ? content.size() : line_end + 1U;

        const std::string_view trimmed = audio_common::trim_whitespace(line);
        if (trimmed.empty()) {
            continue;
        }

        // Format: "01-00: ALC287 Analog : ALC287 Analog : playback 1 : capture 1"
        const std::size_t colon1 = trimmed.find(':');
        if (colon1 == std::string_view::npos) {
            continue;
        }
        const std::string_view num_part = audio_common::trim_whitespace(trimmed.substr(0, colon1));
        const std::size_t dash_pos = num_part.find('-');
        if (dash_pos == std::string_view::npos) {
            continue;
        }

        const auto card_index = audio_common::parse_u32(num_part.substr(0, dash_pos));
        const auto device_index = audio_common::parse_u32(num_part.substr(dash_pos + 1U));
        if (!card_index || !device_index) {
            return fail(errc::malformed_data);
        }

        alsa_pcm_info pcm;
        pcm.card_index = *card_index;
        pcm.device_index = *device_index;

        const std::string_view rest = trimmed.substr(colon1 + 1U);
        const std::size_t colon2 = rest.find(':');
        if (colon2 != std::string_view::npos) {
            pcm.id = std::string(audio_common::trim_whitespace(rest.substr(0, colon2)));
            const std::string_view rest2 = rest.substr(colon2 + 1U);
            const std::size_t colon3 = rest2.find(':');
            if (colon3 != std::string_view::npos) {
                pcm.name = std::string(audio_common::trim_whitespace(rest2.substr(0, colon3)));
                const std::string_view streams = rest2.substr(colon3 + 1U);
                if (audio_common::contains_ignore_case(streams, "playback")) {
                    pcm.has_playback = true;
                }
                if (audio_common::contains_ignore_case(streams, "capture")) {
                    pcm.has_capture = true;
                }
            } else {
                pcm.name = std::string(audio_common::trim_whitespace(rest2));
                if (audio_common::contains_ignore_case(rest2, "playback")) {
                    pcm.has_playback = true;
                }
                if (audio_common::contains_ignore_case(rest2, "capture")) {
                    pcm.has_capture = true;
                }
            }
        } else {
            pcm.id = std::string(audio_common::trim_whitespace(rest));
            pcm.name = pcm.id;
            if (audio_common::contains_ignore_case(rest, "playback")) {
                pcm.has_playback = true;
            }
            if (audio_common::contains_ignore_case(rest, "capture")) {
                pcm.has_capture = true;
            }
        }

        if (audio_common::contains_ignore_case(trimmed, "playback")) {
            pcm.has_playback = true;
        }
        if (audio_common::contains_ignore_case(trimmed, "capture")) {
            pcm.has_capture = true;
        }

        if (!is_valid_utf8(pcm.id) || !is_valid_utf8(pcm.name)) {
            return fail(errc::invalid_encoding);
        }

        pcms.push_back(std::move(pcm));
    }

    return pcms;
}

/// Collects all audio devices from ALSA procfs and sysfs.
inline result<std::vector<::syscape::audio::audio_device>> collect_devices() {
    const char* cards_path = "/proc/asound/cards";
    const char* pcm_path = "/proc/asound/pcm";

    const auto cards_content = linux_platform::read_text_file(cards_path);
    if (!cards_content) {
        if (cards_content.error() !=
            std::error_code(ENOENT, std::generic_category())) {
            return fail(cards_content.error());
        }
        // If /proc/asound is unavailable, check if /sys/class/sound exists
        const linux_platform::directory_handle sound_dir("/sys/class/sound");
        if (!sound_dir.valid()) {
            return fail(errc::not_supported);
        }
        return std::vector<::syscape::audio::audio_device>{};
    }

    const auto cards_res = parse_proc_asound_cards(*cards_content);
    if (!cards_res) {
        return fail(cards_res.error());
    }

    std::unordered_map<std::uint32_t, alsa_card_info> card_map;
    for (const auto& c : *cards_res) {
        card_map.emplace(c.index, c);
    }

    const auto pcm_content = linux_platform::read_text_file(pcm_path);
    std::vector<::syscape::audio::audio_device> devices;

    if (pcm_content) {
        const auto pcms_res = parse_proc_asound_pcm(*pcm_content);
        if (!pcms_res) {
            return fail(pcms_res.error());
        }

        for (const auto& pcm : *pcms_res) {
            ::syscape::audio::audio_device dev;
            dev.id = "hw:" + std::to_string(pcm.card_index) + "," + std::to_string(pcm.device_index);
            dev.name = pcm.name.empty() ? pcm.id : pcm.name;
            dev.state = ::syscape::audio::audio_device_state::unknown;

            if (pcm.has_playback && pcm.has_capture) {
                dev.direction = ::syscape::audio::audio_device_direction::duplex;
            } else if (pcm.has_playback) {
                dev.direction = ::syscape::audio::audio_device_direction::playback;
            } else if (pcm.has_capture) {
                dev.direction = ::syscape::audio::audio_device_direction::capture;
            } else {
                dev.direction = ::syscape::audio::audio_device_direction::unknown;
            }

            const auto it = card_map.find(pcm.card_index);
            if (it != card_map.end()) {
                const auto& card = it->second;
                if (!card.short_name.empty()) {
                    dev.card_name = card.short_name;
                } else if (!card.long_name.empty()) {
                    dev.card_name = card.long_name;
                } else if (!card.id.empty()) {
                    dev.card_name = card.id;
                }
                if (!card.driver.empty()) {
                    dev.driver_name = card.driver;
                }
            }

            devices.push_back(std::move(dev));
        }
    } else if (pcm_content.error() ==
               std::error_code(ENOENT, std::generic_category())) {
        return devices;
    } else {
        return fail(pcm_content.error());
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
    return fail(errc::not_supported);
}

inline result<::syscape::audio::audio_device> default_capture_device() {
    return fail(errc::not_supported);
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

#endif // SYSCAPE_DETAIL_AUDIO_LINUX_HPP
