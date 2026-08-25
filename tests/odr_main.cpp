#include <system_error>

#include <syscape/architecture.hpp>
#include <syscape/audio.hpp>
#include <syscape/bluetooth.hpp>
#include <syscape/camera.hpp>
#include <syscape/connection.hpp>
#include <syscape/cpu.hpp>
#include <syscape/display.hpp>
#include <syscape/error.hpp>
#include <syscape/filesystem.hpp>
#include <syscape/gpu.hpp>
#include <syscape/hardware.hpp>
#include <syscape/input.hpp>
#include <syscape/locale.hpp>
#include <syscape/memory.hpp>
#include <syscape/network.hpp>
#include <syscape/os.hpp>
#include <syscape/power.hpp>
#include <syscape/printer.hpp>
#include <syscape/process.hpp>
#include <syscape/process_list.hpp>
#include <syscape/resource.hpp>
#include <syscape/result.hpp>
#include <syscape/security.hpp>
#include <syscape/sensor.hpp>
#include <syscape/storage.hpp>
#include <syscape/user.hpp>
#include <syscape/virtualization.hpp>
#include <syscape/wifi.hpp>

const std::error_category* other_error_category();
syscape::architecture other_architecture();
bool other_os_backend_callable();
bool other_cpu_backend_callable();
bool other_memory_backend_callable();
bool other_process_backend_callable();
bool other_user_backend_callable();
bool other_filesystem_backend_callable();
bool other_network_backend_callable();
bool other_locale_backend_callable();
bool other_environment_backend_callable();
bool other_resource_backend_callable();
bool other_power_backend_callable();
bool other_storage_backend_callable();
bool other_hardware_backend_callable();
bool other_virtualization_backend_callable();
bool other_gpu_backend_callable();
bool other_display_backend_callable();
bool other_security_backend_callable();
bool other_sensor_backend_callable();
bool other_audio_backend_callable();
bool other_input_backend_callable();
bool other_camera_backend_callable();
bool other_bluetooth_backend_callable();
bool other_wifi_backend_callable();
bool other_printer_backend_callable();
bool other_process_list_backend_callable();
bool other_connection_backend_callable();

int main() {
    const auto languages = syscape::locale::preferred_languages();
    const auto region = syscape::locale::country_region_code();
    const auto zone = syscape::locale::time_zone_identifier();
    static_cast<void>(languages);
    static_cast<void>(region);
    static_cast<void>(zone);
    if (other_error_category() != &syscape::error_category()) {
        return 1;
    }
    if (other_architecture() != syscape::target_architecture()) {
        return 2;
    }
    if (!other_os_backend_callable()) {
        return 3;
    }
    if (!other_cpu_backend_callable()) {
        return 4;
    }
    if (!other_memory_backend_callable()) {
        return 5;
    }
    if (!other_process_backend_callable()) {
        return 6;
    }
    if (!other_user_backend_callable()) {
        return 7;
    }
    if (!other_filesystem_backend_callable()) {
        return 8;
    }
    if (!other_network_backend_callable()) {
        return 9;
    }
    if (!other_locale_backend_callable()) {
        return 11;
    }
    if (!other_environment_backend_callable()) {
        return 12;
    }
    if (!other_resource_backend_callable()) {
        return 13;
    }
    if (!other_power_backend_callable()) {
        return 14;
    }
    if (!other_storage_backend_callable()) {
        return 15;
    }
    if (!other_hardware_backend_callable()) {
        return 16;
    }
    if (!other_virtualization_backend_callable()) {
        return 17;
    }
    if (!other_gpu_backend_callable()) {
        return 18;
    }
    if (!other_display_backend_callable()) {
        return 19;
    }
    if (!other_security_backend_callable()) {
        return 20;
    }
    if (!other_sensor_backend_callable()) {
        return 21;
    }
    if (!other_audio_backend_callable()) {
        return 22;
    }
    if (!other_input_backend_callable()) {
        return 23;
    }
    if (!other_camera_backend_callable()) {
        return 24;
    }
    if (!other_bluetooth_backend_callable()) {
        return 25;
    }
    if (!other_wifi_backend_callable()) {
        return 26;
    }
    if (!other_printer_backend_callable()) {
        return 27;
    }
    if (!other_process_list_backend_callable()) {
        return 28;
    }
    if (!other_connection_backend_callable()) {
        return 29;
    }
    const syscape::result<int> value(7);
    return value && *value == 7 ? 0 : 10;
}
