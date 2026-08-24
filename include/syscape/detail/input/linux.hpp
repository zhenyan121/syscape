#ifndef SYSCAPE_DETAIL_INPUT_LINUX_HPP
#define SYSCAPE_DETAIL_INPUT_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <syscape/input.hpp>
#include <syscape/detail/input/common.hpp>
#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace input_backend {

struct raw_input_entry {
    std::uint16_t bus = 0U;
    std::uint16_t vendor = 0U;
    std::uint16_t product = 0U;
    std::uint16_t version = 0U;
    std::string name;
    std::string phys;
    std::string sysfs;
    std::string uniq;
    std::vector<std::string> handlers;
    std::string prop_mask;
    std::string ev_mask;
    std::string key_mask;
    std::string rel_mask;
    std::string abs_mask;
    std::string sw_mask;
};

/// Splits a string by space delimiters.
inline std::vector<std::string> split_words(std::string_view text) {
    std::vector<std::string> words;
    std::size_t start = 0U;
    while (start < text.size()) {
        while (start < text.size() && static_cast<unsigned char>(text[start]) <= ' ') {
            ++start;
        }
        if (start >= text.size()) {
            break;
        }
        std::size_t end = start;
        while (end < text.size() && static_cast<unsigned char>(text[end]) > ' ') {
            ++end;
        }
        words.emplace_back(text.substr(start, end - start));
        start = end;
    }
    return words;
}

/// Checks if a vector of strings contains a specific needle.
inline bool has_handler(const std::vector<std::string>& handlers, std::string_view needle) noexcept {
    for (const auto& h : handlers) {
        if (h == needle) {
            return true;
        }
    }
    return false;
}

/// Checks if any handler starts with a given prefix (e.g. "event", "mouse", "js").
inline bool has_handler_prefix(const std::vector<std::string>& handlers, std::string_view prefix) noexcept {
    for (const auto& h : handlers) {
        if (h.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

/// Tests a bit in the least-significant word of a Linux hexadecimal bitmap.
inline bool low_bitmap_bit_is_set(std::string_view bitmap, unsigned int bit) noexcept {
    if (bit >= 64U) {
        return false;
    }
    const std::string_view trimmed = input_common::trim_whitespace(bitmap);
    if (trimmed.empty()) {
        return false;
    }
    const std::size_t separator = trimmed.find_last_of(" \t");
    const std::string_view word = separator == std::string_view::npos
                                      ? trimmed
                                      : trimmed.substr(separator + 1U);
    std::uint64_t value = 0U;
    const auto parsed = std::from_chars(
        word.data(), word.data() + word.size(), value, 16);
    return parsed.ec == std::errc() &&
           parsed.ptr == word.data() + word.size() &&
           (value & (std::uint64_t{1U} << bit)) != 0U;
}

/// Validates the space-separated hexadecimal words used by Linux bitmaps.
inline bool is_valid_hex_bitmap(std::string_view bitmap) noexcept {
    const auto words = split_words(bitmap);
    if (words.empty()) {
        return false;
    }
    for (const auto& word : words) {
        std::uint64_t value = 0U;
        const auto parsed = std::from_chars(
            word.data(), word.data() + word.size(), value, 16);
        if (parsed.ec != std::errc() ||
            parsed.ptr != word.data() + word.size()) {
            return false;
        }
    }
    return true;
}

/// Classifies device type from raw properties, name, handlers, and capability masks.
inline ::syscape::input::device_type classify_device(const raw_input_entry& entry) {
    const std::string_view name = entry.name;
    const bool has_keys = low_bitmap_bit_is_set(entry.ev_mask, 1U); // EV_KEY
    const bool has_relative_axes = low_bitmap_bit_is_set(entry.ev_mask, 2U); // EV_REL
    const bool has_absolute_axes = low_bitmap_bit_is_set(entry.ev_mask, 3U); // EV_ABS
    const bool has_switches = low_bitmap_bit_is_set(entry.ev_mask, 5U); // EV_SW
    const bool direct = low_bitmap_bit_is_set(entry.prop_mask, 1U); // INPUT_PROP_DIRECT
    const bool pointer = low_bitmap_bit_is_set(entry.prop_mask, 0U); // INPUT_PROP_POINTER
    const bool buttonpad = low_bitmap_bit_is_set(entry.prop_mask, 2U); // INPUT_PROP_BUTTONPAD

    // Check game controllers / joysticks
    if ((has_keys || has_absolute_axes) &&
        (has_handler_prefix(entry.handlers, "js") ||
         input_common::contains_ignore_case(name, "gamepad") ||
         input_common::contains_ignore_case(name, "joystick") ||
         input_common::contains_ignore_case(name, "controller"))) {
        return ::syscape::input::device_type::gamepad;
    }

    // Check drawing tablets / digitizers
    if (has_absolute_axes &&
        ((direct && pointer) ||
         input_common::contains_ignore_case(name, "tablet") ||
         input_common::contains_ignore_case(name, "wacom") ||
         input_common::contains_ignore_case(name, "digitizer") ||
         input_common::contains_ignore_case(name, "stylus"))) {
        return ::syscape::input::device_type::drawing_tablet;
    }

    // Check touchpads
    if (has_absolute_axes &&
        (buttonpad || (pointer && !direct) ||
         input_common::contains_ignore_case(name, "touchpad") ||
         input_common::contains_ignore_case(name, "trackpad") ||
         input_common::contains_ignore_case(name, "synaptics") ||
         input_common::contains_ignore_case(name, "glidepoint") ||
         input_common::contains_ignore_case(name, "clickpad"))) {
        return ::syscape::input::device_type::touchpad;
    }

    // Check touchscreens
    if (has_absolute_axes &&
        ((direct && !pointer) ||
         input_common::contains_ignore_case(name, "touchscreen") ||
         input_common::contains_ignore_case(name, "touch screen"))) {
        return ::syscape::input::device_type::touchscreen;
    }

    // Check power/sleep/lid buttons and switches
    if (has_switches ||
        (has_keys &&
         (input_common::contains_ignore_case(name, "power button") ||
          input_common::contains_ignore_case(name, "sleep button") ||
          input_common::contains_ignore_case(name, "lid switch") ||
          input_common::contains_ignore_case(name, "video bus") ||
          input_common::contains_ignore_case(name, "switch") ||
          input_common::contains_ignore_case(name, "pwrbtn")))) {
        return ::syscape::input::device_type::button_or_switch;
    }

    // Check mice / pointing devices
    if ((has_relative_axes || has_absolute_axes) &&
        (has_handler_prefix(entry.handlers, "mouse") ||
         input_common::contains_ignore_case(name, "mouse") ||
         input_common::contains_ignore_case(name, "trackball") ||
         input_common::contains_ignore_case(name, "pointing stick") ||
         input_common::contains_ignore_case(name, "trackpoint"))) {
        return ::syscape::input::device_type::mouse;
    }

    // Check keyboards
    if (has_keys &&
        (has_handler(entry.handlers, "kbd") ||
         input_common::contains_ignore_case(name, "keyboard") ||
         input_common::contains_ignore_case(name, "keypad"))) {
        return ::syscape::input::device_type::keyboard;
    }

    return ::syscape::input::device_type::unknown;
}

/// Parses /proc/bus/input/devices content.
inline result<std::vector<::syscape::input::input_device>> parse_proc_bus_input_devices(
    std::string_view content) {
    std::vector<::syscape::input::input_device> devices;
    std::size_t offset = 0U;

    raw_input_entry current;
    bool in_entry = false;

    auto finish_entry = [&]() -> result<void> {
        if (!in_entry) {
            return {};
        }

        // Every string crossing the public boundary must be valid UTF-8.
        if (!detail::is_valid_utf8(current.name) ||
            !detail::is_valid_utf8(current.phys) ||
            !detail::is_valid_utf8(current.sysfs) ||
            !detail::is_valid_utf8(current.uniq)) {
            return fail(errc::malformed_data);
        }
        for (const auto& handler : current.handlers) {
            if (!detail::is_valid_utf8(handler)) {
                return fail(errc::malformed_data);
            }
        }

        ::syscape::input::input_device dev;
        dev.name = current.name;

        // Derive device id (prefer sysfs leaf name e.g. "input0", else primary event handler)
        if (!current.sysfs.empty()) {
            const std::size_t slash = current.sysfs.rfind('/');
            if (slash != std::string_view::npos && slash + 1U < current.sysfs.size()) {
                dev.id = current.sysfs.substr(slash + 1U);
            } else {
                dev.id = current.sysfs;
            }
        } else {
            for (const auto& h : current.handlers) {
                if (h.rfind("event", 0) == 0) {
                    dev.id = h;
                    break;
                }
            }
            if (dev.id.empty() && !current.handlers.empty()) {
                dev.id = current.handlers.front();
            }
        }

        dev.bus = input_common::bus_from_numeric_id(current.bus);
        dev.type = classify_device(current);

        ::syscape::input::input_device_id hw_id;
        hw_id.bus = dev.bus;
        hw_id.vendor_id = current.vendor;
        hw_id.product_id = current.product;
        hw_id.version = current.version;
        dev.hardware_id = hw_id;

        if (!current.phys.empty()) {
            dev.physical_location = current.phys;
        }
        if (!current.sysfs.empty()) {
            dev.sysfs_path = current.sysfs;
        }
        if (!current.uniq.empty()) {
            dev.unique_id = current.uniq;
        }

        dev.handlers = current.handlers;

        devices.push_back(std::move(dev));
        current = raw_input_entry{};
        in_entry = false;
        return {};
    };

    while (offset < content.size()) {
        const std::size_t line_end = content.find('\n', offset);
        const std::string_view line = (line_end == std::string_view::npos)
                                          ? content.substr(offset)
                                          : content.substr(offset, line_end - offset);
        offset = (line_end == std::string_view::npos) ? content.size() : line_end + 1U;

        const std::string_view trimmed = input_common::trim_whitespace(line);
        if (trimmed.empty()) {
            const auto finish_res = finish_entry();
            if (!finish_res) {
                return fail(finish_res.error());
            }
            continue;
        }

        if (trimmed.size() < 3U || trimmed[1] != ':') {
            return fail(errc::malformed_data);
        }

        const char tag = trimmed[0];
        const std::string_view payload = input_common::trim_whitespace(trimmed.substr(2));

        if (tag == 'I') {
            if (in_entry) {
                const auto finish_res = finish_entry();
                if (!finish_res) {
                    return fail(finish_res.error());
                }
            }
            in_entry = true;

            // Parse Bus=XXXX Vendor=XXXX Product=XXXX Version=XXXX
            const auto parse_key_hex = [&](std::string_view key) -> result<std::uint16_t> {
                const std::size_t pos = payload.find(key);
                if (pos == std::string_view::npos) {
                    return fail(errc::malformed_data);
                }
                const std::size_t val_start = pos + key.size();
                std::size_t val_end = val_start;
                while (val_end < payload.size() && static_cast<unsigned char>(payload[val_end]) > ' ') {
                    ++val_end;
                }
                return input_common::parse_hex_u16(payload.substr(val_start, val_end - val_start));
            };

            const auto bus_res = parse_key_hex("Bus=");
            const auto vendor_res = parse_key_hex("Vendor=");
            const auto prod_res = parse_key_hex("Product=");
            const auto ver_res = parse_key_hex("Version=");

            if (!bus_res || !vendor_res || !prod_res || !ver_res) {
                return fail(errc::malformed_data);
            }

            current.bus = *bus_res;
            current.vendor = *vendor_res;
            current.product = *prod_res;
            current.version = *ver_res;
        } else if (!in_entry) {
            return fail(errc::malformed_data);
        } else if (tag == 'N') {
            // N: Name="AT Translated Set 2 keyboard"
            const std::size_t name_pos = payload.find("Name=");
            if (name_pos == std::string_view::npos) {
                return fail(errc::malformed_data);
            }
            std::string_view val = payload.substr(name_pos + 5U);
            if (val.size() >= 2U && val.front() == '"' && val.back() == '"') {
                val = val.substr(1, val.size() - 2U);
            }
            current.name = std::string(val);
        } else if (tag == 'P') {
            // P: Phys=isa0060/serio0/input0
            if (payload.rfind("Phys=", 0) == 0) {
                current.phys = std::string(payload.substr(5));
            }
        } else if (tag == 'S') {
            // S: Sysfs=/devices/platform/i8042/serio0/input/input3
            if (payload.rfind("Sysfs=", 0) == 0) {
                current.sysfs = std::string(payload.substr(6));
            }
        } else if (tag == 'U') {
            // U: Uniq=...
            if (payload.rfind("Uniq=", 0) == 0) {
                current.uniq = std::string(payload.substr(5));
            }
        } else if (tag == 'H') {
            // H: Handlers=sysrq kbd leds event3
            if (payload.rfind("Handlers=", 0) == 0) {
                current.handlers = split_words(payload.substr(9));
            }
        } else if (tag == 'B') {
            // B: PROP=... or B: EV=...
            const std::size_t equals = payload.find('=');
            if (equals == std::string_view::npos || equals == 0U ||
                !is_valid_hex_bitmap(payload.substr(equals + 1U))) {
                return fail(errc::malformed_data);
            }
            if (payload.rfind("PROP=", 0) == 0) {
                current.prop_mask = std::string(payload.substr(5));
            } else if (payload.rfind("EV=", 0) == 0) {
                current.ev_mask = std::string(payload.substr(3));
            } else if (payload.rfind("KEY=", 0) == 0) {
                current.key_mask = std::string(payload.substr(4));
            } else if (payload.rfind("REL=", 0) == 0) {
                current.rel_mask = std::string(payload.substr(4));
            } else if (payload.rfind("ABS=", 0) == 0) {
                current.abs_mask = std::string(payload.substr(4));
            } else if (payload.rfind("SW=", 0) == 0) {
                current.sw_mask = std::string(payload.substr(3));
            }
        } else {
            return fail(errc::malformed_data);
        }
    }

    const auto finish_res = finish_entry();
    if (!finish_res) {
        return fail(finish_res.error());
    }

    return devices;
}

inline result<std::vector<::syscape::input::input_device>> devices() {
    const auto content_res = linux_platform::read_text_file("/proc/bus/input/devices");
    if (!content_res) {
        if (content_res.error() == std::error_code(ENOENT, std::generic_category())) {
            // Check if /sys/class/input exists
            const linux_platform::directory_handle input_dir("/sys/class/input");
            if (input_dir.valid()) {
                return std::vector<::syscape::input::input_device>{};
            }
            if (input_dir.error() == ENOENT) {
                return fail(errc::not_supported);
            }
            return fail(std::error_code(input_dir.error(), std::generic_category()));
        }
        return fail(content_res.error());
    }

    return parse_proc_bus_input_devices(*content_res);
}

inline result<std::vector<::syscape::input::input_device>> keyboards() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_by_type(*all, ::syscape::input::device_type::keyboard);
}

inline result<std::vector<::syscape::input::input_device>> mice() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_by_type(*all, ::syscape::input::device_type::mouse);
}

inline result<std::vector<::syscape::input::input_device>> touch_devices() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_touch_devices(*all);
}

inline result<std::vector<::syscape::input::input_device>> gamepads() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_gamepads(*all);
}

inline result<std::size_t> device_count() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return all->size();
}

} // namespace input_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_INPUT_LINUX_HPP
