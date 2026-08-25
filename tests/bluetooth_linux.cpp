#include <cassert>
#include <cstddef>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <syscape/bluetooth.hpp>
#include <syscape/detail/bluetooth/common.hpp>
#include <syscape/detail/bluetooth/linux.hpp>
#include <syscape/detail/utf8.hpp>

namespace {

std::string write_temporary_file(const char* data, std::size_t size) {
    char path[] = "/tmp/syscape-bt-XXXXXX";
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

void write_file(const std::string& path, const char* data, std::size_t size) {
    const int descriptor =
        ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    assert(descriptor >= 0);
    std::size_t written = 0U;
    while (written < size) {
        const ssize_t count =
            ::write(descriptor, data + written, size - written);
        assert(count > 0);
        written += static_cast<std::size_t>(count);
    }
    assert(::close(descriptor) == 0);
}

void test_common_helpers() {
    using namespace syscape::detail::bluetooth_common;

    // Test trim_whitespace
    assert(trim_whitespace("   hello   ") == "hello");
    assert(trim_whitespace("") == "");
    assert(trim_whitespace("   ") == "");

    // Test natural_less
    assert(natural_less("hci0", "hci1"));
    assert(natural_less("hci2", "hci10"));
    assert(!natural_less("hci10", "hci2"));
    assert(!natural_less("hci02", "hci2"));
    assert(natural_less("hci2", "hci02"));

    // Test parse_int
    const auto u32_dec = parse_int<std::uint32_t>("  42  ");
    assert(u32_dec && *u32_dec == 42U);
    assert(!parse_int<std::uint32_t>(""));
    assert(!parse_int<std::uint32_t>("xyz"));

    const auto u16_hex = parse_int<std::uint16_t>("0x1A2B", 16);
    assert(u16_hex && *u16_hex == 0x1A2BU);
    const auto u16_hex2 = parse_int<std::uint16_t>("1a2b", 16);
    assert(u16_hex2 && *u16_hex2 == 0x1A2BU);

    // Test normalize_mac_address
    const auto norm1 = normalize_mac_address("00:1a:7d:da:71:13");
    assert(norm1 && *norm1 == "00:1A:7D:DA:71:13");
    const auto norm2 = normalize_mac_address("00-1A-7D-DA-71-13");
    assert(norm2 && *norm2 == "00:1A:7D:DA:71:13");
    const auto norm3 = normalize_mac_address("001a7dda7113");
    assert(norm3 && *norm3 == "00:1A:7D:DA:71:13");
    assert(!normalize_mac_address(""));
    assert(!normalize_mac_address("00:11:22"));
    assert(!normalize_mac_address("invalid_mac_addr"));

    // Test format_mac_bytes
    const std::uint8_t raw_mac[6] = {0x00, 0x1A, 0x7D, 0xDA, 0x71, 0x13};
    assert(format_mac_bytes(raw_mac, false) == "00:1A:7D:DA:71:13");
    assert(format_mac_bytes(raw_mac, true) == "13:71:DA:7D:1A:00");

    // Test decode_major_device_class
    using syscape::bluetooth::major_device_class;
    assert(decode_major_device_class(0x000000) == major_device_class::miscellaneous);
    assert(decode_major_device_class(0x000104) == major_device_class::computer);
    assert(decode_major_device_class(0x000204) == major_device_class::phone);
    assert(decode_major_device_class(0x000300) == major_device_class::network_access_point);
    assert(decode_major_device_class(0x000408) == major_device_class::audio_video);
    assert(decode_major_device_class(0x000540) == major_device_class::peripheral);
    assert(decode_major_device_class(0x000680) == major_device_class::imaging);
    assert(decode_major_device_class(0x000700) == major_device_class::wearable);
    assert(decode_major_device_class(0x000800) == major_device_class::toy);
    assert(decode_major_device_class(0x000900) == major_device_class::health);
    assert(decode_major_device_class(0x001F00) == major_device_class::unknown);
}

void test_sysfs_parsing_errors() {
    using syscape::detail::bluetooth_backend::read_sysfs_string;
    using syscape::detail::bluetooth_backend::read_sysfs_int;

    const auto missing =
        read_sysfs_string("/tmp/syscape-bt-nonexistent-file-xyz");
    assert(missing && !missing->has_value());

    const std::string valid_path = write_temporary_file(" Intel Wireless\n", 16U);
    const auto valid = read_sysfs_string(valid_path);
    assert(::unlink(valid_path.c_str()) == 0);
    assert(valid && valid->has_value() && **valid == "Intel Wireless");

    const char invalid_bytes[] = {static_cast<char>(0xC3U), '(', '\n'};
    const std::string invalid_path =
        write_temporary_file(invalid_bytes, sizeof(invalid_bytes));
    const auto invalid = read_sysfs_string(invalid_path);
    assert(::unlink(invalid_path.c_str()) == 0);
    assert(!invalid);
    assert(invalid.error() == syscape::errc::invalid_encoding);

    const std::string malformed_int_path = write_temporary_file("not_a_number\n", 13U);
    const auto malformed_int = read_sysfs_int<int>(malformed_int_path);
    assert(::unlink(malformed_int_path.c_str()) == 0);
    assert(!malformed_int);
    assert(malformed_int.error() == syscape::errc::malformed_data);
}

void test_rfkill_adapter_correlation() {
    using syscape::bluetooth::adapter_power_state;
    using syscape::detail::bluetooth_backend::apply_hci_flags;
    using syscape::detail::bluetooth_backend::detect_rfkill_state;

    char root_template[] = "/tmp/syscape-bt-rfkill-XXXXXX";
    const char* root_value = ::mkdtemp(root_template);
    assert(root_value != nullptr);
    const std::string root(root_value);
    const std::string adapter_dir = root + "/adapter";
    const std::string rfkill_root = root + "/rfkill";
    const std::string rfkill_entry = rfkill_root + "/rfkill0";
    assert(::mkdir(adapter_dir.c_str(), 0700) == 0);
    assert(::mkdir(rfkill_root.c_str(), 0700) == 0);
    assert(::mkdir(rfkill_entry.c_str(), 0700) == 0);
    write_file(rfkill_entry + "/name", "hci1\n", 5U);
    write_file(rfkill_entry + "/type", "bluetooth\n", 10U);
    write_file(rfkill_entry + "/soft", "1\n", 2U);
    write_file(rfkill_entry + "/hard", "0\n", 2U);

    const auto unrelated = detect_rfkill_state(
        "hci0", adapter_dir, rfkill_root);
    assert(unrelated && *unrelated == adapter_power_state::unknown);

    write_file(rfkill_entry + "/name", "hci0\n", 5U);
    const auto matching = detect_rfkill_state(
        "hci0", adapter_dir, rfkill_root);
    assert(matching && *matching == adapter_power_state::blocked);

    write_file(rfkill_entry + "/soft", "0\n", 2U);
    const auto unblocked = detect_rfkill_state(
        "hci0", adapter_dir, rfkill_root);
    assert(unblocked && *unblocked == adapter_power_state::unknown);

    syscape::bluetooth::adapter_info powered_off;
    powered_off.power_state = *unblocked;
    apply_hci_flags(0U, powered_off);
    assert(powered_off.power_state == adapter_power_state::off);
    assert(powered_off.is_connectable == false);
    assert(powered_off.is_discoverable == false);

    syscape::bluetooth::adapter_info powered_on;
    powered_on.power_state = adapter_power_state::unknown;
    apply_hci_flags((1U << 0U) | (1U << 3U) | (1U << 4U), powered_on);
    assert(powered_on.power_state == adapter_power_state::on);
    assert(powered_on.is_connectable == true);
    assert(powered_on.is_discoverable == true);

    syscape::bluetooth::adapter_info blocked;
    blocked.power_state = adapter_power_state::blocked;
    apply_hci_flags(1U << 0U, blocked);
    assert(blocked.power_state == adapter_power_state::blocked);

    assert(::unlink((rfkill_entry + "/hard").c_str()) == 0);
    assert(::unlink((rfkill_entry + "/soft").c_str()) == 0);
    assert(::unlink((rfkill_entry + "/type").c_str()) == 0);
    assert(::unlink((rfkill_entry + "/name").c_str()) == 0);
    assert(::rmdir(rfkill_entry.c_str()) == 0);
    assert(::rmdir(rfkill_root.c_str()) == 0);
    assert(::rmdir(adapter_dir.c_str()) == 0);
    assert(::rmdir(root.c_str()) == 0);
}

void test_pairing_record_errors() {
    using syscape::detail::bluetooth_backend::paired_devices;

    char root_template[] = "/tmp/syscape-bt-paired-XXXXXX";
    const char* root_value = ::mkdtemp(root_template);
    assert(root_value != nullptr);
    const std::string root(root_value);
    const std::string adapter_dir = root + "/00:11:22:33:44:55";
    const std::string device_dir = adapter_dir + "/AA:BB:CC:DD:EE:FF";
    const std::string info_path = device_dir + "/info";
    assert(::mkdir(adapter_dir.c_str(), 0700) == 0);
    assert(::mkdir(device_dir.c_str(), 0700) == 0);

    const char invalid_name[] = {
        '[', 'G', 'e', 'n', 'e', 'r', 'a', 'l', ']', '\n',
        'N', 'a', 'm', 'e', '=', static_cast<char>(0xC3U), '(', '\n'};
    write_file(info_path, invalid_name, sizeof(invalid_name));
    const auto invalid = paired_devices(root.c_str());
    assert(!invalid);
    assert(invalid.error() == syscape::errc::invalid_encoding);

    constexpr char unpaired[] = "[General]\nBonded=false\n";
    write_file(info_path, unpaired, sizeof(unpaired) - 1U);
    const auto filtered = paired_devices(root.c_str());
    assert(filtered && filtered->empty());

    assert(::unlink(info_path.c_str()) == 0);
    assert(::rmdir(device_dir.c_str()) == 0);
    assert(::rmdir(adapter_dir.c_str()) == 0);
    assert(::rmdir(root.c_str()) == 0);
}

void test_live_queries() {
    const auto adapters_res = syscape::bluetooth::adapters();
    assert(adapters_res);

    const auto count_res = syscape::bluetooth::adapter_count();
    assert(count_res);
    assert(*count_res == adapters_res->size());

    const auto default_res = syscape::bluetooth::default_adapter();
    if (adapters_res->empty()) {
        assert(!default_res);
        assert(default_res.error() == syscape::errc::not_found);
    } else {
        assert(default_res);
        assert(!default_res->id.empty());
        assert(!default_res->name.empty());
    }

    for (const auto& ad : *adapters_res) {
        assert(!ad.id.empty());
        assert(!ad.name.empty());
        if (ad.address.has_value()) {
            assert(ad.address->size() == 17U);
        }
    }

    const auto conn_res = syscape::bluetooth::connected_devices();
    assert(conn_res || static_cast<bool>(conn_res.error()));

    const auto paired_res = syscape::bluetooth::paired_devices();
    if (!paired_res) {
        assert(paired_res.error() == syscape::errc::permission_denied);
    }
}

} // namespace

int main() {
    test_common_helpers();
    test_sysfs_parsing_errors();
    test_rfkill_adapter_correlation();
    test_pairing_record_errors();
    test_live_queries();
    return 0;
}
