#include <cassert>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/input.hpp>
#include <syscape/detail/input/common.hpp>
#include <syscape/detail/input/linux.hpp>
#include <syscape/detail/utf8.hpp>

namespace {

void test_synthetic_proc_bus_input_parsing() {
    using namespace syscape::detail::input_backend;

    const std::string_view sample =
        "I: Bus=0019 Vendor=0000 Product=0001 Version=0000\n"
        "N: Name=\"Power Button\"\n"
        "P: Phys=PNP0C0C/button/input0\n"
        "S: Sysfs=/devices/platform/PNP0C0C:00/input/input0\n"
        "U: Uniq=\n"
        "H: Handlers=kbd event0 \n"
        "B: PROP=0\n"
        "B: EV=3\n"
        "B: KEY=8000 10000000000000 0\n"
        "\n"
        "I: Bus=0019 Vendor=0000 Product=0005 Version=0000\n"
        "N: Name=\"Lid Switch\"\n"
        "P: Phys=PNP0C0D/button/input0\n"
        "S: Sysfs=/devices/platform/PNP0C0D:00/input/input1\n"
        "U: Uniq=\n"
        "H: Handlers=event1 \n"
        "B: PROP=0\n"
        "B: EV=21\n"
        "B: SW=1\n"
        "\n"
        "I: Bus=0011 Vendor=0001 Product=0001 Version=ab83\n"
        "N: Name=\"AT Translated Set 2 keyboard\"\n"
        "P: Phys=isa0060/serio0/input0\n"
        "S: Sysfs=/devices/platform/i8042/serio0/input/input3\n"
        "U: Uniq=\n"
        "H: Handlers=sysrq kbd leds event3 \n"
        "B: PROP=0\n"
        "B: EV=120013\n"
        "B: KEY=402000007 ff803078f800d001 feffffdfffcfffff fffffffffffffffe\n"
        "B: MSC=10\n"
        "B: LED=7\n"
        "\n"
        "I: Bus=0003 Vendor=1532 Product=0098 Version=0111\n"
        "N: Name=\"Razer Optical Mouse\"\n"
        "P: Phys=usb-0000:05:00.4-2/input0\n"
        "S: Sysfs=/devices/pci0000:00/usb3/input/input12\n"
        "U: Uniq=RZ01-000123\n"
        "H: Handlers=mouse0 event12 \n"
        "B: PROP=0\n"
        "B: EV=17\n"
        "B: KEY=1f0000 0 0 0 0\n"
        "B: REL=903\n"
        "B: MSC=10\n"
        "\n"
        "I: Bus=0018 Vendor=27c6 Product=01e0 Version=0000\n"
        "N: Name=\"GXTP5100:00 Touchpad\"\n"
        "P: Phys=i2c-GXTP5100:00/input0\n"
        "S: Sysfs=/devices/platform/i2c-0/input/input24\n"
        "U: Uniq=\n"
        "H: Handlers=mouse1 event21 \n"
        "B: PROP=5\n"
        "B: EV=b\n"
        "B: KEY=e520 10000 0 0 0 0\n"
        "B: ABS=260800000000003\n"
        "\n"
        "I: Bus=0003 Vendor=045e Product=028e Version=0110\n"
        "N: Name=\"Microsoft X-Box 360 pad\"\n"
        "P: Phys=usb-0000:05:00.4-3/input0\n"
        "S: Sysfs=/devices/pci0000:00/usb3/input/input25\n"
        "U: Uniq=\n"
        "H: Handlers=js0 event22 \n"
        "B: PROP=0\n"
        "B: EV=20000b\n"
        "B: KEY=7fff0000 0 0 0 0\n"
        "B: ABS=3003f\n"
        "\n"
        "I: Bus=0003 Vendor=056a Product=037a Version=0100\n"
        "N: Name=\"Wacom Intuos Pro Pen\"\n"
        "P: Phys=usb-0000:05:00.4-4/input0\n"
        "S: Sysfs=/devices/pci0000:00/usb3/input/input26\n"
        "U: Uniq=\n"
        "H: Handlers=mouse2 event23 \n"
        "B: PROP=1\n"
        "B: EV=1f\n"
        "B: KEY=1c03 0 0 0 0\n"
        "B: ABS=10000000003\n";

    const auto res = parse_proc_bus_input_devices(sample);
    assert(res);
    assert(res->size() == 7U);

    // Power Button
    const auto& pwr = (*res)[0];
    assert(pwr.id == "input0");
    assert(pwr.name == "Power Button");
    assert(pwr.type == syscape::input::device_type::button_or_switch);
    assert(pwr.bus == syscape::input::bus_type::virtual_bus);
    assert(pwr.hardware_id.has_value());
    assert(pwr.hardware_id->vendor_id == 0U);
    assert(pwr.hardware_id->product_id == 1U);
    assert(!pwr.is_integrated.has_value());

    // Lid Switch
    const auto& lid = (*res)[1];
    assert(lid.id == "input1");
    assert(lid.name == "Lid Switch");
    assert(lid.type == syscape::input::device_type::button_or_switch);

    // Keyboard
    const auto& kbd = (*res)[2];
    assert(kbd.id == "input3");
    assert(kbd.name == "AT Translated Set 2 keyboard");
    assert(kbd.type == syscape::input::device_type::keyboard);
    assert(kbd.bus == syscape::input::bus_type::isa_serio);
    assert(kbd.hardware_id.has_value());
    assert(kbd.hardware_id->vendor_id == 1U);
    assert(kbd.hardware_id->product_id == 1U);
    assert(kbd.hardware_id->version == 0xab83U);
    assert(!kbd.is_integrated.has_value());

    // Mouse
    const auto& mouse = (*res)[3];
    assert(mouse.id == "input12");
    assert(mouse.name == "Razer Optical Mouse");
    assert(mouse.type == syscape::input::device_type::mouse);
    assert(mouse.bus == syscape::input::bus_type::usb);
    assert(mouse.hardware_id.has_value());
    assert(mouse.hardware_id->vendor_id == 0x1532U);
    assert(mouse.hardware_id->product_id == 0x0098U);
    assert(mouse.unique_id.has_value() && *mouse.unique_id == "RZ01-000123");

    // Touchpad
    const auto& touch = (*res)[4];
    assert(touch.id == "input24");
    assert(touch.name == "GXTP5100:00 Touchpad");
    assert(touch.type == syscape::input::device_type::touchpad);
    assert(touch.bus == syscape::input::bus_type::i2c);
    assert(!touch.is_integrated.has_value());

    // Gamepad
    const auto& pad = (*res)[5];
    assert(pad.id == "input25");
    assert(pad.name == "Microsoft X-Box 360 pad");
    assert(pad.type == syscape::input::device_type::gamepad);
    assert(pad.bus == syscape::input::bus_type::usb);

    // Drawing Tablet
    const auto& tab = (*res)[6];
    assert(tab.id == "input26");
    assert(tab.name == "Wacom Intuos Pro Pen");
    assert(tab.type == syscape::input::device_type::drawing_tablet);
    assert(tab.bus == syscape::input::bus_type::usb);

    // Filter tests
    const auto kbds = syscape::detail::input_common::filter_by_type(*res, syscape::input::device_type::keyboard);
    assert(kbds.size() == 1U);
    assert(kbds[0].name == "AT Translated Set 2 keyboard");

    const auto mice = syscape::detail::input_common::filter_by_type(*res, syscape::input::device_type::mouse);
    assert(mice.size() == 1U);
    assert(mice[0].name == "Razer Optical Mouse");

    const auto touches = syscape::detail::input_common::filter_touch_devices(*res);
    assert(touches.size() == 2U); // Touchpad + Wacom Tablet

    const auto gamepads = syscape::detail::input_common::filter_gamepads(*res);
    assert(gamepads.size() == 1U);
    assert(gamepads[0].name == "Microsoft X-Box 360 pad");

    // Test empty
    const auto empty_res = parse_proc_bus_input_devices("");
    assert(empty_res && empty_res->empty());

    // Test malformed line
    const auto malformed = parse_proc_bus_input_devices("X: bad line\n");
    assert(!malformed && malformed.error() == syscape::errc::malformed_data);

    // Test malformed I: line
    const auto bad_i = parse_proc_bus_input_devices("I: Bus=ZZZZ\n");
    assert(!bad_i && bad_i.error() == syscape::errc::malformed_data);

    std::string invalid_utf8 =
        "I: Bus=0003 Vendor=0001 Product=0002 Version=0003\n"
        "N: Name=\"Valid name\"\n"
        "P: Phys=";
    invalid_utf8.push_back(static_cast<char>(0xFF));
    invalid_utf8 += "\nH: Handlers=event0\nB: EV=1\n";
    const auto bad_text = parse_proc_bus_input_devices(invalid_utf8);
    assert(!bad_text && bad_text.error() == syscape::errc::malformed_data);

    const auto bad_bitmap = parse_proc_bus_input_devices(
        "I: Bus=0003 Vendor=0001 Product=0002 Version=0003\n"
        "N: Name=\"Device\"\n"
        "H: Handlers=event0\n"
        "B: EV=not-hex\n");
    assert(!bad_bitmap && bad_bitmap.error() == syscape::errc::malformed_data);
}

void test_capability_classification() {
    using syscape::detail::input_backend::classify_device;
    using syscape::detail::input_backend::raw_input_entry;

    raw_input_entry touchscreen;
    touchscreen.name = "Generic HID surface";
    touchscreen.ev_mask = "b";   // EV_SYN, EV_KEY, EV_ABS
    touchscreen.prop_mask = "2"; // INPUT_PROP_DIRECT
    assert(classify_device(touchscreen) ==
           syscape::input::device_type::touchscreen);

    raw_input_entry touchpad;
    touchpad.name = "Generic HID surface";
    touchpad.ev_mask = "b";
    touchpad.prop_mask = "1"; // INPUT_PROP_POINTER
    assert(classify_device(touchpad) == syscape::input::device_type::touchpad);

    raw_input_entry tablet;
    tablet.name = "Generic HID surface";
    tablet.ev_mask = "b";
    tablet.prop_mask = "3"; // INPUT_PROP_POINTER | INPUT_PROP_DIRECT
    assert(classify_device(tablet) ==
           syscape::input::device_type::drawing_tablet);

    raw_input_entry speaker;
    speaker.name = "PC Speaker";
    speaker.handlers = {"kbd", "event19"};
    speaker.ev_mask = "40001"; // EV_SYN | EV_SND, but no EV_KEY
    assert(classify_device(speaker) == syscape::input::device_type::unknown);

    raw_input_entry keyboard;
    keyboard.name = "Generic receiver";
    keyboard.handlers = {"kbd", "event20"};
    keyboard.ev_mask = "3"; // EV_SYN | EV_KEY
    assert(classify_device(keyboard) == syscape::input::device_type::keyboard);

    assert(syscape::detail::input_common::bus_from_numeric_id(0x0018U) ==
           syscape::input::bus_type::i2c);
    assert(syscape::detail::input_common::bus_from_numeric_id(0x001CU) ==
           syscape::input::bus_type::unknown);
    assert(syscape::detail::input_common::bus_from_numeric_id(0x001DU) ==
           syscape::input::bus_type::unknown);
}

void test_live_input_queries() {
    const auto all = syscape::input::devices();
    if (!all) {
        // Can be not_supported if procfs input devices is absent
        assert(all.error() == syscape::errc::not_supported ||
               all.error() == syscape::errc::permission_denied);
        return;
    }

    const auto count = syscape::input::device_count();
    assert(count);
    assert(*count == all->size());

    const auto kbds = syscape::input::keyboards();
    assert(kbds);
    assert(kbds->size() <= all->size());

    const auto mice = syscape::input::mice();
    assert(mice);
    assert(mice->size() <= all->size());

    const auto touches = syscape::input::touch_devices();
    assert(touches);
    assert(touches->size() <= all->size());

    const auto pads = syscape::input::gamepads();
    assert(pads);
    assert(pads->size() <= all->size());

    for (const auto& dev : *all) {
        assert(!dev.id.empty());
        assert(!dev.name.empty());
        assert(syscape::detail::is_valid_utf8(dev.id));
        assert(syscape::detail::is_valid_utf8(dev.name));
        if (dev.physical_location) {
            assert(syscape::detail::is_valid_utf8(*dev.physical_location));
        }
        if (dev.sysfs_path) {
            assert(syscape::detail::is_valid_utf8(*dev.sysfs_path));
        }
        if (dev.unique_id) {
            assert(syscape::detail::is_valid_utf8(*dev.unique_id));
        }
        for (const auto& h : dev.handlers) {
            assert(syscape::detail::is_valid_utf8(h));
        }
    }
}

} // namespace

int main() {
    test_synthetic_proc_bus_input_parsing();
    test_capability_classification();
    test_live_input_queries();
    return 0;
}
