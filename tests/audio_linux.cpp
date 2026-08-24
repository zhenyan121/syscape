#include <cassert>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/audio.hpp>
#include <syscape/detail/audio/common.hpp>
#include <syscape/detail/audio/linux.hpp>
#include <syscape/detail/utf8.hpp>

namespace {

void test_synthetic_proc_cards_parsing() {
    using namespace syscape::detail::audio_backend;

    const std::string_view sample_cards =
        " 0 [NVidia         ]: HDA-Intel - HDA NVidia\n"
        "                      HDA NVidia at 0xc4000000 irq 95\n"
        " 1 [Generic        ]: HDA-Intel - HD-Audio Generic\n"
        "                      HD-Audio Generic at 0xc48c0000 irq 96\n";

    const auto res = parse_proc_asound_cards(sample_cards);
    assert(res);
    assert(res->size() == 2U);

    assert((*res)[0].index == 0U);
    assert((*res)[0].id == "NVidia");
    assert((*res)[0].driver == "HDA-Intel");
    assert((*res)[0].short_name == "HDA NVidia");
    assert((*res)[0].long_name == "HDA NVidia at 0xc4000000 irq 95");

    assert((*res)[1].index == 1U);
    assert((*res)[1].id == "Generic");
    assert((*res)[1].driver == "HDA-Intel");
    assert((*res)[1].short_name == "HD-Audio Generic");
    assert((*res)[1].long_name == "HD-Audio Generic at 0xc48c0000 irq 96");

    // Test empty
    const auto empty_res = parse_proc_asound_cards("");
    assert(empty_res && empty_res->empty());

    // Test no soundcards marker
    const auto none_res = parse_proc_asound_cards("--- no soundcards ---\n");
    assert(none_res && none_res->empty());

    // Test malformed bracket
    const auto malformed = parse_proc_asound_cards(" 0 [broken: HDA-Intel\n");
    assert(!malformed && malformed.error() == syscape::errc::malformed_data);
}

void test_synthetic_proc_pcm_parsing() {
    using namespace syscape::detail::audio_backend;

    const std::string_view sample_pcm =
        "00-03: HDMI 0 : HDMI 0 : playback 1\n"
        "00-07: HDMI 1 : HDMI 1 : playback 1\n"
        "01-00: ALC287 Analog : ALC287 Analog : playback 1 : capture 1\n"
        "01-02: ALC287 Mic : ALC287 Mic : capture 1\n";

    const auto res = parse_proc_asound_pcm(sample_pcm);
    assert(res);
    assert(res->size() == 4U);

    assert((*res)[0].card_index == 0U);
    assert((*res)[0].device_index == 3U);
    assert((*res)[0].id == "HDMI 0");
    assert((*res)[0].name == "HDMI 0");
    assert((*res)[0].has_playback);
    assert(!(*res)[0].has_capture);

    assert((*res)[1].card_index == 0U);
    assert((*res)[1].device_index == 7U);
    assert((*res)[1].id == "HDMI 1");
    assert((*res)[1].has_playback);
    assert(!(*res)[1].has_capture);

    assert((*res)[2].card_index == 1U);
    assert((*res)[2].device_index == 0U);
    assert((*res)[2].id == "ALC287 Analog");
    assert((*res)[2].name == "ALC287 Analog");
    assert((*res)[2].has_playback);
    assert((*res)[2].has_capture);

    assert((*res)[3].card_index == 1U);
    assert((*res)[3].device_index == 2U);
    assert((*res)[3].id == "ALC287 Mic");
    assert(!(*res)[3].has_playback);
    assert((*res)[3].has_capture);

    // Empty pcm
    const auto empty = parse_proc_asound_pcm("");
    assert(empty && empty->empty());
}

void test_direction_filtering() {
    using namespace syscape::audio;
    using namespace syscape::detail::audio_common;

    std::vector<audio_device> devs;

    audio_device d1;
    d1.id = "hw:0,0";
    d1.name = "Speakers";
    d1.direction = audio_device_direction::playback;
    d1.state = audio_device_state::active;
    d1.is_default_playback = true;
    devs.push_back(d1);

    audio_device d2;
    d2.id = "hw:0,1";
    d2.name = "Microphone";
    d2.direction = audio_device_direction::capture;
    d2.state = audio_device_state::active;
    d2.is_default_capture = true;
    devs.push_back(d2);

    audio_device d3;
    d3.id = "hw:1,0";
    d3.name = "Headset";
    d3.direction = audio_device_direction::duplex;
    d3.state = audio_device_state::active;
    devs.push_back(d3);

    const auto playbacks = filter_by_direction(devs, audio_device_direction::playback);
    assert(playbacks.size() == 2U);
    assert(playbacks[0].id == "hw:0,0");
    assert(playbacks[1].id == "hw:1,0");

    const auto captures = filter_by_direction(devs, audio_device_direction::capture);
    assert(captures.size() == 2U);
    assert(captures[0].id == "hw:0,1");
    assert(captures[1].id == "hw:1,0");

    const auto def_play = find_default_device(devs, audio_device_direction::playback);
    assert(def_play);
    assert(def_play->id == "hw:0,0");

    const auto def_cap = find_default_device(devs, audio_device_direction::capture);
    assert(def_cap);
    assert(def_cap->id == "hw:0,1");

    devs[2].is_default_playback = true;
    devs[0].is_default_playback.reset();
    devs[1].is_default_capture.reset();
    const auto duplex_playback =
        find_default_device(devs, audio_device_direction::playback);
    assert(duplex_playback && duplex_playback->id == "hw:1,0");
    assert(!find_default_device(devs, audio_device_direction::capture));
}

void test_live_audio_queries() {
    using namespace syscape::audio;

    const auto all = devices();
    if (!all) {
        assert(all.error() == syscape::errc::not_supported);
        return;
    }

    const auto count = device_count();
    assert(count && *count == all->size());

    for (const auto& dev : *all) {
        assert(!dev.id.empty());
        assert(syscape::detail::is_valid_utf8(dev.id));
        assert(!dev.name.empty());
        assert(syscape::detail::is_valid_utf8(dev.name));
        assert(dev.state == audio_device_state::unknown);
        assert(!dev.is_default_playback);
        assert(!dev.is_default_capture);
        assert(!dev.sample_rate_hz);
        assert(!dev.min_sample_rate_hz);
        assert(!dev.max_sample_rate_hz);
        if (dev.card_name) {
            assert(syscape::detail::is_valid_utf8(*dev.card_name));
        }
        if (dev.driver_name) {
            assert(syscape::detail::is_valid_utf8(*dev.driver_name));
        }
    }

    const auto playbacks = playback_devices();
    assert(playbacks);
    for (const auto& dev : *playbacks) {
        assert(dev.direction == audio_device_direction::playback ||
               dev.direction == audio_device_direction::duplex);
    }

    const auto captures = capture_devices();
    assert(captures);
    for (const auto& dev : *captures) {
        assert(dev.direction == audio_device_direction::capture ||
               dev.direction == audio_device_direction::duplex);
    }

    const auto def_play = default_playback_device();
    assert(!def_play && def_play.error() == syscape::errc::not_supported);

    const auto def_cap = default_capture_device();
    assert(!def_cap && def_cap.error() == syscape::errc::not_supported);
}

} // namespace

int main() {
    test_synthetic_proc_cards_parsing();
    test_synthetic_proc_pcm_parsing();
    test_direction_filtering();
    test_live_audio_queries();
    return 0;
}
