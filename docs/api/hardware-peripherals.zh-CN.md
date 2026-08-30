[English](hardware-peripherals.md) | [简体中文](hardware-peripherals.zh-CN.md)

# 硬件与外设 API 参考 (Hardware & Peripherals)

硬件与外设头文件提供系统硬件标识、主板、BIOS/UEFI 固件、GPU 显卡、显示器、电池电源、温度传感器、音频设备、摄像头、输入设备及打印机队列的全面自省接口。

本组所有头文件均要求 **托管完整版 (Hosted Full)** 配置与 **严格 C++17** 标准。

---

## 1. 系统标识与硬件清单 (`<syscape/hardware.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::hardware`

### 类型与枚举

#### 枚举类型
- `enum class form_factor`: `unknown`, `other`, `desktop`, `low_profile_desktop`, `pizza_box`, `mini_tower`, `tower`, `portable`, `laptop`, `notebook`, `hand_held`, `docking_station`, `all_in_one`, `sub_notebook`, `space_saving`, `lunch_box`, `main_server`, `expansion_chassis`, `sub_chassis`, `bus_expansion_chassis`, `peripheral_chassis`, `raid_chassis`, `rack_mount_chassis`, `sealed_case_pc`, `multi_system`, `compact_pci`, `advanced_tca`, `blade`, `blade_enclosure`, `tablet`, `convertible`, `detachable`, `iot_gateway`, `embedded_pc`, `mini_pc`, `stick_pc`。
- `enum class memory_device_type`: `unknown`, `other`, `dram`, `ddr`, `ddr2`, `ddr3`, `ddr4`, `ddr5`, `lpddr`, `lpddr2`, `lpddr3`, `lpddr4`, `lpddr5`, `hbm`, `hbm2`, `hbm3`, `nvdimm`。

#### 核心结构体
```cpp
struct pci_device {
    std::string address;
    std::uint16_t vendor_id = 0U;
    std::uint16_t device_id = 0U;
    std::optional<std::uint16_t> subsystem_vendor_id;
    std::optional<std::uint16_t> subsystem_device_id;
    std::string vendor_name;
    std::string device_name;
    std::string driver_name;
};

struct usb_device {
    std::string address;
    std::uint16_t vendor_id = 0U;
    std::uint16_t product_id = 0U;
    std::string manufacturer_name;
    std::string product_name;
    std::string serial_number;
};

struct memory_device {
    std::string locator;
    std::string bank_locator;
    std::string manufacturer;
    std::string serial_number;
    std::string part_number;
    memory_device_type type = memory_device_type::unknown;
    std::optional<std::uint64_t> size_bytes;
    std::optional<std::uint32_t> speed_mts;
};
```

### 函数接口

```cpp
result<std::string> system_manufacturer();
result<std::string> system_product_name();
result<std::string> system_product_version();

result<std::string> motherboard_manufacturer();
result<std::string> motherboard_product_name();
result<std::string> motherboard_version();

result<std::string> firmware_vendor();
result<std::string> firmware_version();
result<std::string> firmware_release_date();

result<form_factor> chassis_form_factor();
result<std::string> hardware_uuid();

result<std::vector<pci_device>> pci_devices();
result<std::vector<usb_device>> usb_devices();
result<std::vector<memory_device>> memory_devices();
```

---

## 2. GPU 与图形适配器 (`<syscape/gpu.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::gpu`

### 类型与枚举

```cpp
enum class gpu_vendor : std::uint8_t {
    unknown, amd, nvidia, intel, apple, arm_mali,
    qualcomm_adreno, broadcom_videocore, imagination_powervr,
    microsoft, vmware, virtio, other
};

struct gpu_device {
    std::string id;
    std::optional<std::string> name;
    gpu_vendor vendor = gpu_vendor::unknown;
    std::string vendor_name;
    std::optional<std::uint32_t> vendor_id;
    std::optional<std::uint32_t> device_id;
    std::optional<std::string> driver;
    std::optional<std::uint64_t> vram_bytes;
    std::optional<bool> is_primary;
};
```

### 函数接口

```cpp
result<std::vector<gpu_device>> devices();
result<std::size_t> device_count();
result<gpu_device> primary_device();
const char* vendor_name(gpu_vendor vendor) noexcept;
```

---

## 3. 显示器与屏幕 (`<syscape/display.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::display`

### 类型与结构体

```cpp
enum class display_orientation : std::uint8_t {
    unknown, landscape, portrait, landscape_flipped, portrait_flipped
};

struct rectangle {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct display_mode {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    double refresh_rate_hz = 0.0;
    std::uint32_t bit_depth = 0;
};

struct display_info {
    std::string id;
    std::string name;
    rectangle bounds;
    rectangle work_area;
    display_mode current_mode;
    std::vector<display_mode> supported_modes;
    double scale_factor = 1.0;
    display_orientation orientation = display_orientation::unknown;
    bool is_primary = false;
    bool is_internal = false;
};
```

### 函数接口

```cpp
result<std::vector<display_info>> displays();
result<std::size_t> display_count();
result<display_info> primary_display();
```

---

## 4. 电源与电池 (`<syscape/power.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::power`

### 类型与枚举

```cpp
enum class power_source_type : std::uint8_t {
    unknown, ac_mains, battery, ups, wireless, usb
};

enum class battery_charging_state : std::uint8_t {
    unknown, charging, discharging, full, not_charging
};

struct battery_entry {
    std::string name;
    std::optional<std::string> manufacturer;
    std::optional<std::string> model;
    std::optional<std::string> serial_number;
    battery_charging_state charging_state = battery_charging_state::unknown;
    std::optional<std::uint8_t> charge_percent;
    std::optional<std::uint8_t> health_percent;
    std::optional<std::uint64_t> energy_design_mwh;
    std::optional<std::uint64_t> energy_full_mwh;
    std::optional<std::uint64_t> energy_current_mwh;
    std::optional<std::uint32_t> voltage_mv;
    std::optional<std::int32_t> power_rate_mw;
    std::optional<double> temperature_celsius;
    std::optional<std::uint32_t> cycle_count;
    std::optional<std::uint64_t> time_remaining_seconds;
};

struct power_source_entry {
    power_source_type type = power_source_type::unknown;
    std::string description;
    bool is_online = false;
};
```

### 函数接口

```cpp
result<std::vector<battery_entry>> batteries();
result<std::vector<power_source_entry>> power_sources();
result<bool> external_power_online();
result<std::uint64_t> seconds_until_empty();
```

---

## 5. 温度与风扇传感器 (`<syscape/sensor.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::sensor`

### 类型与结构体

```cpp
enum class temperature_sensor_type : std::uint8_t {
    unknown, cpu, gpu, storage, motherboard, ambient, power_supply, other
};

enum class thermal_zone_type : std::uint8_t {
    unknown, cpu, gpu, acpi, soc, battery, ambient, other
};

struct temperature_sensor {
    std::string label;
    temperature_sensor_type type = temperature_sensor_type::unknown;
    double current_celsius = 0.0;
    std::optional<double> max_celsius;
    std::optional<double> critical_celsius;
    std::optional<std::string> chip_name;
    std::optional<std::string> device_id;
};

struct fan_sensor {
    std::string label;
    std::uint32_t current_rpm = 0U;
    std::optional<std::uint32_t> min_rpm;
    std::optional<std::uint32_t> max_rpm;
    std::optional<std::uint32_t> target_rpm;
    std::optional<std::string> chip_name;
    std::optional<std::string> device_id;
};

struct thermal_zone {
    std::string type_name;
    thermal_zone_type type = thermal_zone_type::unknown;
    double current_celsius = 0.0;
    std::optional<double> passive_celsius;
    std::optional<double> critical_celsius;
    std::optional<std::string> zone_id;
    bool enabled = true;
};
```

### 函数接口

```cpp
result<std::vector<temperature_sensor>> temperatures();
result<std::vector<fan_sensor>> fans();
result<std::vector<thermal_zone>> thermal_zones();
```

---

## 6. 音频设备与端点 (`<syscape/audio.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::audio`

### 类型与结构体

```cpp
enum class audio_device_direction : std::uint8_t {
    unknown, playback, capture, duplex
};

enum class audio_device_state : std::uint8_t {
    unknown, active, disabled, unplugged, not_present
};

struct audio_device {
    std::string id;
    std::string name;
    audio_device_direction direction = audio_device_direction::unknown;
    audio_device_state state = audio_device_state::unknown;
    std::optional<std::string> card_name;
    std::optional<std::string> driver_name;
    std::optional<std::uint32_t> playback_channels;
    std::optional<std::uint32_t> capture_channels;
    std::optional<std::uint32_t> sample_rate_hz;
    std::optional<std::uint32_t> min_sample_rate_hz;
    std::optional<std::uint32_t> max_sample_rate_hz;
    std::optional<bool> is_default_playback;
    std::optional<bool> is_default_capture;
};
```

### 函数接口

```cpp
result<std::vector<audio_device>> devices();
result<std::vector<audio_device>> playback_devices();
result<std::vector<audio_device>> capture_devices();
result<audio_device> default_playback_device();
result<audio_device> default_capture_device();
result<std::size_t> device_count();
```

---

## 7. 摄像头设备 (`<syscape/camera.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::camera`

### 类型与结构体

```cpp
enum class camera_facing : std::uint8_t {
    unknown, front, back, external
};

enum class camera_connection : std::uint8_t {
    unknown, built_in, usb, pci, virtual_connection
};

struct camera_device_id {
    std::optional<std::uint16_t> vendor_id;
    std::optional<std::uint16_t> product_id;
    std::optional<std::uint16_t> revision;
};

struct camera_capabilities {
    std::optional<bool> has_video_capture;
    std::optional<bool> has_video_output;
    std::optional<bool> has_metadata_capture;
    std::optional<bool> has_streaming;
    std::optional<bool> has_touch_device;
};

struct camera_device {
    std::string id;
    std::string name;
    std::optional<std::string> device_path;
    std::optional<std::string> sysfs_path;
    std::optional<std::string> driver;
    std::optional<std::string> card;
    std::optional<std::string> bus_info;
    camera_facing facing = camera_facing::unknown;
    camera_connection connection = camera_connection::unknown;
    std::optional<camera_device_id> hardware_id;
    std::optional<bool> is_integrated;
    std::optional<camera_capabilities> capabilities;
};
```

### 函数接口

```cpp
result<std::vector<camera_device>> devices();
result<std::size_t> device_count();
result<std::vector<camera_device>> capture_devices();
result<camera_device> default_device();
```

---

## 8. 输入设备 (`<syscape/input.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::input`

### 类型与结构体

```cpp
enum class device_type : std::uint8_t {
    unknown, keyboard, mouse, touchpad, touchscreen, joystick, gamepad, drawing_tablet, button_or_switch
};

enum class bus_type : std::uint8_t {
    unknown, usb, bluetooth, pci, i2c, isa_serio, virtual_bus
};

struct input_device_id {
    bus_type bus = bus_type::unknown;
    std::uint16_t vendor_id = 0U;
    std::uint16_t product_id = 0U;
    std::uint16_t version = 0U;
};

struct input_device {
    std::string id;
    std::string name;
    device_type type = device_type::unknown;
    bus_type bus = bus_type::unknown;
    std::optional<input_device_id> hardware_id;
    std::optional<std::string> physical_location;
    std::optional<std::string> sysfs_path;
    std::optional<std::string> unique_id;
    std::vector<std::string> handlers;
    std::optional<bool> is_integrated;
};
```

### 函数接口

```cpp
result<std::vector<input_device>> devices();
result<std::vector<input_device>> keyboards();
result<std::vector<input_device>> mice();
result<std::vector<input_device>> touch_devices();
result<std::vector<input_device>> gamepads();
result<std::size_t> device_count();
```

---

## 9. 打印机 (`<syscape/printer.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::printer`

### 类型与结构体

```cpp
enum class printer_state : std::uint8_t {
    unknown, idle, processing, stopped
};

enum class printer_type : std::uint8_t {
    unknown, local, network, virtual_printer
};

struct printer_capabilities {
    std::optional<bool> color;
    std::optional<bool> duplex;
    std::optional<bool> copies;
    std::optional<bool> collate;
    std::vector<std::string> supported_media;
    std::vector<std::string> supported_resolutions;
    std::optional<std::uint32_t> max_copies;
};

struct printer_info {
    std::string id;
    std::string name;
    std::string driver_name;
    std::string location;
    std::string description;
    std::string uri;
    printer_type type = printer_type::unknown;
    printer_state state = printer_state::unknown;
    std::optional<bool> is_default;
    std::optional<bool> is_shared;
    std::optional<bool> is_accepting_jobs;
    std::optional<std::uint32_t> queued_job_count;
    printer_capabilities capabilities;
};
```

### 函数接口

```cpp
result<std::vector<printer_info>> printers();
result<std::size_t> printer_count();
result<printer_info> default_printer();
result<printer_info> find_printer(std::string_view name_or_id);
```
