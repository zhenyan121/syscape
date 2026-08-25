#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

#include <syscape/camera.hpp>
#include <syscape/detail/camera/common.hpp>
#include <syscape/detail/camera/linux.hpp>
#include <syscape/detail/utf8.hpp>

namespace {

std::string write_temporary_file(const char* data, std::size_t size) {
    char path[] = "/tmp/syscape-camera-XXXXXX";
    const int descriptor = ::mkstemp(path);
    assert(descriptor >= 0);
    std::size_t written = 0U;
    while (written < size) {
        const ssize_t count =
            ::write(descriptor, data + written, size - written);
        assert(count > 0);
        written += static_cast<std::size_t>(count);
    }
    assert(::close(descriptor) == 0);
    return path;
}

void test_common_helpers() {
    using namespace syscape::detail::camera_common;

    // Test trim_whitespace
    assert(trim_whitespace("   hello   ") == "hello");
    assert(trim_whitespace("") == "");
    assert(trim_whitespace("   ") == "");

    // Test contains_ignore_case
    assert(contains_ignore_case("Integrated Camera", "camera"));
    assert(contains_ignore_case("Integrated Camera", "CAMERA"));
    assert(contains_ignore_case("Logitech", "LOGI"));
    assert(!contains_ignore_case("Logitech", "Camera"));

    assert(natural_less("video2", "video10"));
    assert(!natural_less("video10", "video2"));
    assert(natural_less("video2", "video02"));
    assert(natural_less("v4l-subdev9", "video1"));

    // Test parse_u32
    const auto u32_val = parse_u32("  42  ");
    assert(u32_val && *u32_val == 42U);
    assert(!parse_u32(""));
    assert(!parse_u32("abc"));

    // Test parse_hex_u16
    const auto hex_val = parse_hex_u16("5986");
    assert(hex_val && *hex_val == 0x5986U);
    const auto hex_prefix = parse_hex_u16("0x2175");
    assert(hex_prefix && *hex_prefix == 0x2175U);
    assert(!parse_hex_u16(""));
    assert(!parse_hex_u16("xyz"));

    // Test detect_facing_from_panel
    using namespace syscape::detail::camera_backend;
    assert(detect_facing_from_panel("top") ==
           syscape::camera::camera_facing::front);
    assert(detect_facing_from_panel("front") ==
           syscape::camera::camera_facing::front);
    assert(detect_facing_from_panel("back") ==
           syscape::camera::camera_facing::back);
    assert(detect_facing_from_panel("bottom") ==
           syscape::camera::camera_facing::back);
    assert(detect_facing_from_panel("external") ==
           syscape::camera::camera_facing::external);
    assert(detect_facing_from_panel("unknown") ==
           syscape::camera::camera_facing::unknown);
}

void test_sysfs_text_errors() {
    using syscape::detail::camera_backend::read_sysfs_string;

    const auto missing =
        read_sysfs_string("/tmp/syscape-camera-file-that-must-not-exist");
    assert(missing && !missing->has_value());

    const std::string valid_path = write_temporary_file(" Camera Name\n", 13U);
    const auto valid = read_sysfs_string(valid_path);
    assert(::unlink(valid_path.c_str()) == 0);
    assert(valid && valid->has_value() && **valid == "Camera Name");

    const char invalid_bytes[] = {static_cast<char>(0xC3U), '(', '\n'};
    const std::string invalid_path =
        write_temporary_file(invalid_bytes, sizeof(invalid_bytes));
    const auto invalid = read_sysfs_string(invalid_path);
    assert(::unlink(invalid_path.c_str()) == 0);
    assert(!invalid);
    assert(invalid.error() == syscape::errc::invalid_encoding);

    const std::string empty_path = write_temporary_file("", 0U);
    const auto empty = read_sysfs_string(empty_path);
    assert(::unlink(empty_path.c_str()) == 0);
    assert(empty && !empty->has_value());

    const std::string whitespace_path = write_temporary_file(" \t\n", 3U);
    const auto whitespace = read_sysfs_string(whitespace_path);
    assert(::unlink(whitespace_path.c_str()) == 0);
    assert(whitespace && !whitespace->has_value());
}

void test_capture_filter() {
    using namespace syscape::camera;

    camera_device d1;
    d1.id = "video0";
    d1.name = "Integrated Camera";
    d1.is_integrated = true;
    d1.facing = camera_facing::front;
    camera_capabilities caps1;
    caps1.has_video_capture = true;
    d1.capabilities = caps1;

    camera_device d2;
    d2.id = "video1";
    d2.name = "Metadata Node";
    d2.is_integrated = true;
    camera_capabilities caps2;
    caps2.has_video_capture = false;
    caps2.has_metadata_capture = true;
    d2.capabilities = caps2;

    camera_device d3;
    d3.id = "video2";
    d3.name = "Unreadable Camera";

    const std::vector<camera_device> devs = {d1, d2, d3};

    const auto filtered =
        syscape::detail::camera_common::filter_capture_devices(devs);
    assert(filtered.size() == 1U);
    assert(filtered[0].id == "video0");

    const unsigned char valid_text[] = {'u', 'v', 'c', 0U};
    const auto valid = syscape::detail::camera_backend::v4l2_text(
        valid_text, sizeof(valid_text));
    assert(valid && valid->has_value() && **valid == "uvc");

    const unsigned char unterminated[] = {'u', 'v', 'c'};
    const auto malformed = syscape::detail::camera_backend::v4l2_text(
        unterminated, sizeof(unterminated));
    assert(!malformed);
    assert(malformed.error() == syscape::errc::malformed_data);

    const unsigned char invalid_utf8[] = {0xC3U, 0x28U, 0U};
    const auto invalid = syscape::detail::camera_backend::v4l2_text(
        invalid_utf8, sizeof(invalid_utf8));
    assert(!invalid);
    assert(invalid.error() == syscape::errc::invalid_encoding);
}

void test_live_camera_queries() {
    const auto devs = syscape::camera::devices();
    const auto count = syscape::camera::device_count();
    assert(devs || devs.error());
    assert(count || count.error());
    if (devs && count) {
        assert(*count == devs->size());
    }

    const auto capture = syscape::camera::capture_devices();
    if (capture) {
        if (devs) {
            assert(capture->size() <= devs->size());
        }
        for (const auto& dev : *capture) {
            assert(dev.capabilities.has_value());
            assert(dev.capabilities->has_video_capture.value_or(false));
        }
    } else {
        assert(capture.error());
    }

    if (devs) {
        for (const auto& dev : *devs) {
            assert(!dev.id.empty());
            assert(syscape::detail::is_valid_utf8(dev.id));
            assert(!dev.name.empty());
            assert(syscape::detail::is_valid_utf8(dev.name));

            if (dev.device_path) {
                assert(!dev.device_path->empty());
                assert(syscape::detail::is_valid_utf8(*dev.device_path));
            }
            if (dev.sysfs_path) {
                assert(!dev.sysfs_path->empty());
                assert(syscape::detail::is_valid_utf8(*dev.sysfs_path));
            }
            if (dev.driver) {
                assert(!dev.driver->empty());
                assert(syscape::detail::is_valid_utf8(*dev.driver));
            }
            if (dev.card) {
                assert(!dev.card->empty());
                assert(syscape::detail::is_valid_utf8(*dev.card));
            }
            if (dev.bus_info) {
                assert(!dev.bus_info->empty());
                assert(syscape::detail::is_valid_utf8(*dev.bus_info));
            }
        }
    }

    const auto def = syscape::camera::default_device();
    assert(!def);
    assert(def.error() == syscape::errc::not_supported);
}

} // namespace

int main() {
    test_common_helpers();
    test_sysfs_text_errors();
    test_capture_filter();
    test_live_camera_queries();
    return 0;
}
