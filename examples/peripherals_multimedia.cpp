#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <syscape/audio.hpp>
#include <syscape/bluetooth.hpp>
#include <syscape/camera.hpp>
#include <syscape/input.hpp>
#include <syscape/printer.hpp>
#include <syscape/sensor.hpp>

namespace {

const char* audio_dir_name(syscape::audio::audio_device_direction dir) {
    switch (dir) {
    case syscape::audio::audio_device_direction::playback: return "Output / Playback";
    case syscape::audio::audio_device_direction::capture: return "Input / Microphone";
    case syscape::audio::audio_device_direction::duplex: return "Duplex (In/Out)";
    case syscape::audio::audio_device_direction::unknown: return "Unknown";
    }
    return "Unknown";
}

const char* input_type_name(syscape::input::device_type type) {
    switch (type) {
    case syscape::input::device_type::keyboard: return "Keyboard";
    case syscape::input::device_type::mouse: return "Mouse";
    case syscape::input::device_type::touchpad: return "Touchpad";
    case syscape::input::device_type::touchscreen: return "Touchscreen";
    case syscape::input::device_type::gamepad: return "Gamepad";
    case syscape::input::device_type::joystick: return "Joystick";
    case syscape::input::device_type::drawing_tablet: return "Drawing Tablet";
    case syscape::input::device_type::button_or_switch: return "Button/Switch";
    case syscape::input::device_type::unknown: return "Unknown Input";
    }
    return "Unknown Input";
}

const char* camera_facing_name(syscape::camera::camera_facing facing) {
    switch (facing) {
    case syscape::camera::camera_facing::front: return "Front (User-Facing)";
    case syscape::camera::camera_facing::back: return "Back (Rear-Facing)";
    case syscape::camera::camera_facing::external: return "External USB Webcam";
    case syscape::camera::camera_facing::unknown: return "Integrated / Unknown";
    }
    return "Unknown";
}

} // namespace

int main() {
    std::cout << "=== Syscape Peripherals & Multimedia Example ===" << std::endl;

    // Thermal & Fan Sensors
    std::cout << "\n[Hardware Thermal Sensors & Cooling Fans]" << std::endl;
    if (const auto temps = syscape::sensor::temperatures()) {
        for (const auto& s : *temps) {
            std::cout << "  Temp [" << s.label << "]: "
                      << std::fixed << std::setprecision(1) << s.current_celsius << " °C";
            if (s.max_celsius) {
                std::cout << " (High: " << *s.max_celsius << " °C)";
            }
            if (s.critical_celsius) {
                std::cout << " (Crit: " << *s.critical_celsius << " °C)";
            }
            std::cout << std::endl;
        }
    }
    if (const auto fans = syscape::sensor::fans()) {
        for (const auto& f : *fans) {
            std::cout << "  Fan [" << f.label << "]: "
                      << f.current_rpm << " RPM" << std::endl;
        }
    }

    // Audio Endpoints
    std::cout << "\n[Audio Input & Output Endpoints]" << std::endl;
    if (const auto audio_devs = syscape::audio::devices()) {
        for (const auto& dev : *audio_devs) {
            std::cout << "  Audio: " << dev.name
                      << " [" << audio_dir_name(dev.direction) << "]";
            if (dev.is_default_playback && *dev.is_default_playback) {
                std::cout << " [Default Output]";
            }
            if (dev.is_default_capture && *dev.is_default_capture) {
                std::cout << " [Default Mic]";
            }
            std::cout << std::endl;
            if (dev.sample_rate_hz) {
                std::cout << "    Sample Rate: " << *dev.sample_rate_hz << " Hz" << std::endl;
            }
            if (dev.playback_channels) {
                std::cout << "    Channels:    " << *dev.playback_channels << " channels" << std::endl;
            }
        }
    }

    // Video Cameras
    std::cout << "\n[Video Capture & Cameras]" << std::endl;
    if (const auto cams = syscape::camera::devices()) {
        for (const auto& cam : *cams) {
            std::cout << "  Camera: " << cam.name
                      << " (" << camera_facing_name(cam.facing) << ")";
            if (cam.driver) {
                std::cout << " [Driver: " << *cam.driver << "]";
            }
            std::cout << std::endl;
        }
    }

    // Input Devices
    std::cout << "\n[Human Input Devices (HID)]" << std::endl;
    if (const auto inputs = syscape::input::devices()) {
        for (const auto& in : *inputs) {
            std::cout << "  Input: " << in.name
                      << " (" << input_type_name(in.type) << ")" << std::endl;
        }
    }

    // Printers
    std::cout << "\n[Printer Queues & Destinations]" << std::endl;
    if (const auto default_p = syscape::printer::default_printer()) {
        std::cout << "  Default Printer: " << default_p->name << std::endl;
    }
    if (const auto printers = syscape::printer::printers()) {
        for (const auto& p : *printers) {
            std::cout << "  Printer: " << p.name
                      << (p.is_default ? " [Default]" : "")
                      << (p.location.empty() ? "" : " at " + p.location)
                      << std::endl;
        }
    }

    // Bluetooth
    std::cout << "\n[Bluetooth Radios & Paired Devices]" << std::endl;
    if (const auto bt_adapters = syscape::bluetooth::adapters()) {
        for (const auto& ad : *bt_adapters) {
            std::cout << "  Bluetooth Adapter [" << ad.id << "]: " << ad.name
                      << " (Radio: "
                      << (ad.power_state == syscape::bluetooth::adapter_power_state::on ? "ON" : "OFF/Blocked")
                      << ")" << std::endl;
        }
    }
    if (const auto paired = syscape::bluetooth::paired_devices()) {
        for (const auto& d : *paired) {
            std::cout << "    Paired Device: " << (d.name ? *d.name : "(Unknown Bluetooth Device)")
                      << " [" << d.address << "] "
                      << (d.is_connected && *d.is_connected ? "(Connected)" : "(Disconnected)")
                      << std::endl;
        }
    }

    return 0;
}
