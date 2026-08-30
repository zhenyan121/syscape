[English](system-core.md) | [简体中文](system-core.zh-CN.md)

# 系统核心 API 参考 (System Core)

系统核心头文件提供操作系统标识、处理器拓扑与缓存、物理与虚拟内存、进程执行上下文、全系统进程清单、用户账户与会话、挂载文件系统及物理存储设备等系统底层状态的深度自省接口。

本组所有头文件均要求 **托管完整版 (Hosted Full)** 配置与 **严格 C++17** 标准。

---

## 1. 操作系统查询 (`<syscape/os.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::os`

### 函数接口

```cpp
result<std::string> product_name();
result<std::string> product_version();
result<std::string> build_identifier();
result<std::string> kernel_name();
result<std::string> kernel_version();
result<std::string> host_name();
result<std::string> boot_identifier();
result<std::chrono::milliseconds> uptime();
result<std::chrono::system_clock::time_point> boot_time();
```

---

## 2. CPU 与计算拓扑 (`<syscape/cpu.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::cpu`

### 类型与枚举

#### `enum class cache_kind : std::uint8_t`
- `data`: 纯数据缓存。
- `instruction`: 纯指令缓存。
- `unified`: 统一指令与数据缓存。
- `trace`: 译码执行跟踪缓存（非可寻址物理内存）。
- `unknown`: 平台未明确声明具体缓存类型的实例。

#### `struct usage_snapshot`
系统自开机以来的累积处理器 Tick 计数：
```cpp
struct usage_snapshot {
    std::uint64_t user_ticks;
    std::uint64_t system_ticks;
    std::uint64_t idle_ticks;
};
```

#### `struct cache_information`
描述单个独立的物理缓存实例：
```cpp
struct cache_information {
    std::uint32_t level;
    cache_kind kind;
    std::uint64_t instance_size_bytes;
    std::uint32_t line_size_bytes;
    std::uint32_t associativity_ways;
    std::uint32_t sets_count;
    std::uint32_t shared_logical_processor_count;
};
```

### 函数接口

```cpp
result<std::vector<std::string>> vendor_identifiers();
result<std::vector<std::string>> model_names();
result<std::uint32_t> online_logical_processor_count();
result<std::uint32_t> online_physical_core_count();
result<std::uint32_t> online_processor_package_count();

result<std::uint32_t> minimum_frequency_khz();
result<std::uint32_t> maximum_frequency_khz();
result<std::vector<std::uint32_t>> current_frequencies_khz();

result<usage_snapshot> cumulative_processor_usage();
result<std::vector<cache_information>> cache_descriptors();
result<std::vector<std::string>> instruction_set_features();
```

---

## 3. 系统内存 (`<syscape/memory.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::memory`

### 类型与结构体

#### `struct swap_information`
```cpp
struct swap_information {
    std::uint64_t total_bytes;
    std::uint64_t free_bytes;
};
```

#### `struct commit_information`
```cpp
struct commit_information {
    std::uint64_t committed_bytes;
    std::uint64_t commit_limit_bytes;
};
```

#### `struct huge_page_pool_information`
```cpp
struct huge_page_pool_information {
    std::uint64_t total_count;
    std::uint64_t free_count;
};
```

#### `struct pressure_sample` 与 `struct memory_pressure_status`
```cpp
struct pressure_sample {
    std::uint64_t average10_micro_percent;
    std::uint64_t average60_micro_percent;
    std::uint64_t average300_micro_percent;
    std::uint64_t total_microseconds;
};

struct memory_pressure_status {
    pressure_sample some;
    bool has_full;
    pressure_sample full;
};
```

### 函数接口

```cpp
result<std::uint64_t> page_size_bytes();
result<std::uint64_t> physical_memory_bytes();
result<std::uint64_t> available_memory_bytes();
result<swap_information> swap_status();
result<commit_information> commit_status();
result<std::uint64_t> huge_page_size_bytes();
result<huge_page_pool_information> huge_page_pool_status();
result<std::uint32_t> memory_load_percent();
result<memory_pressure_status> memory_pressure();
```

---

## 4. 当前进程 (`<syscape/process.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::process`

### 类型与结构体

#### `struct cpu_times`
```cpp
struct cpu_times {
    std::chrono::nanoseconds user;
    std::chrono::nanoseconds system;
};
```

#### `struct memory_usage_info`
```cpp
struct memory_usage_info {
    std::uint64_t resident_bytes;
    std::uint64_t virtual_bytes;
};
```

#### `enum class resource_kind`
- `core_file_size`, `cpu_time`, `file_size`, `open_files`, `stack_size`, `address_space`。

#### `struct resource_limits`
```cpp
struct resource_limit_amount {
    std::uint64_t amount;
    bool unlimited;
};

struct resource_limits {
    resource_limit_amount soft;
    resource_limit_amount hard;
};
```

### 函数接口

```cpp
result<std::uint32_t> process_id();
result<std::uint32_t> parent_process_id();
result<std::string> executable_path();
result<std::vector<std::string>> command_line();
result<std::string> working_directory();
result<std::chrono::system_clock::time_point> start_time();
result<cpu_times> cpu_time();
result<memory_usage_info> memory_usage();
result<std::uint32_t> thread_count();
result<int> priority();
result<std::vector<std::uint32_t>> cpu_affinity();
result<resource_limits> resource_limit(resource_kind kind);
```

---

## 5. 全系统进程列表 (`<syscape/process_list.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::process_list`

### 类型与结构体

#### `enum class process_state : std::uint8_t`
- `unknown`, `running`, `sleeping`, `stopped`, `zombie`。

#### `struct process_entry`
```cpp
struct process_entry {
    std::uint32_t pid = 0;
    std::optional<std::uint32_t> ppid;
    std::optional<std::uint32_t> uid;
    std::optional<std::uint32_t> gid;
    std::optional<std::string> user_name;
    std::optional<std::string> name;
    std::optional<std::string> executable_path;
    std::optional<std::vector<std::string>> command_line;
    std::optional<std::string> working_directory;
    process_state state = process_state::unknown;
    std::optional<std::chrono::nanoseconds> user_cpu_time;
    std::optional<std::chrono::nanoseconds> kernel_cpu_time;
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::uint64_t> resident_memory_bytes;
    std::optional<std::uint64_t> virtual_memory_bytes;
    std::optional<std::uint32_t> thread_count;
    std::optional<int> priority;
};
```

### 函数接口

```cpp
result<std::vector<process_entry>> processes();
result<std::uint32_t> process_count();
result<process_entry> find_process(std::uint32_t pid);
result<std::vector<process_entry>> find_processes_by_name(std::string_view name);
```

---

## 6. 用户身份与会话 (`<syscape/user.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::user`

### 类型与枚举

```cpp
enum class privilege_state {
    unknown,
    unprivileged,
    privileged
};

enum class session_type {
    unknown,
    console,
    graphical,
    terminal,
    remote
};

enum class session_state {
    unknown,
    active,
    disconnected,
    locked
};

struct session_info {
    std::string user_name;
    std::string terminal_device;
    std::string host_name;
    session_type type = session_type::unknown;
    session_state state = session_state::unknown;
    std::optional<std::chrono::system_clock::time_point> login_time;
    std::optional<std::uint32_t> session_id;
};
```

### 函数接口

```cpp
result<std::uint32_t> real_user_id();
result<std::uint32_t> effective_user_id();
result<std::uint32_t> real_group_id();
result<std::uint32_t> effective_group_id();
result<std::vector<std::uint32_t>> supplementary_groups();
result<privilege_state> privilege();

result<std::string> login_name();
result<std::string> user_name();
result<std::string> home_directory();
result<std::string> shell();

result<std::vector<session_info>> sessions();
result<std::vector<std::string>> logged_in_users();
```

---

## 7. 文件系统与挂载点 (`<syscape/filesystem.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::filesystem`

### 类型与结构体

#### `struct mount_entry`
```cpp
struct mount_entry {
    std::string source;
    std::string mount_point;
    std::string file_system_type;
};
```

#### `struct space_info`
```cpp
struct space_info {
    std::uint64_t capacity_bytes;
    std::uint64_t free_bytes;
    std::uint64_t available_bytes;
    std::uint64_t block_size_bytes;
    bool read_only;
};
```

#### `struct path_length_limit`
```cpp
struct path_length_limit {
    std::uint64_t length;
    bool indeterminate;
};
```

### 函数接口

```cpp
result<std::vector<mount_entry>> mounts();
result<space_info> space(const std::string& path);
result<path_length_limit> max_component_length(const std::string& path);
result<path_length_limit> max_path_length(const std::string& path);
result<std::string> volume_id(const std::string& path);
```

---

## 8. 物理存储与分区 (`<syscape/storage.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::storage`

### 类型与枚举

#### 枚举类型
- `enum class bus_type`: `unknown`, `scsi`, `sata`, `sas`, `ata`, `atapi`, `usb`, `firewire`, `fibre_channel`, `iscsi`, `raid`, `nvme`, `sd`, `mmc`, `virtual_media`。
- `enum class partition_scheme`: `unknown`, `mbr`, `gpt`, `apple`, `raw`。
- `enum class drive_health_status`: `unknown`, `healthy`, `warning`, `critical`。

#### 核心结构体
```cpp
struct drive_entry {
    std::string identifier;
    bool has_vendor;
    std::string vendor;
    bool has_model;
    std::string model;
    bool has_firmware_revision;
    std::string firmware_revision;
    bus_type bus;
    bool has_capacity_bytes;
    std::uint64_t capacity_bytes;
    bool has_logical_sector_size_bytes;
    std::uint32_t logical_sector_size_bytes;
    bool has_physical_sector_size_bytes;
    std::uint32_t physical_sector_size_bytes;
    bool has_rotational;
    bool rotational;
    bool removable;
};

struct partition_entry {
    std::string identifier;
    std::string disk_identifier;
    std::uint32_t partition_number = 0U;
    bool has_start_offset_bytes = false;
    std::uint64_t start_offset_bytes = 0U;
    bool has_size_bytes = false;
    std::uint64_t size_bytes = 0U;
    partition_scheme scheme = partition_scheme::unknown;
    bool has_type_identifier = false;
    std::string type_identifier;
    bool has_name = false;
    std::string name;
    bool has_uuid = false;
    std::string uuid;
    bool has_filesystem_type = false;
    std::string filesystem_type;
    bool is_read_only = false;
    bool is_bootable = false;
    bool is_mounted = false;
    std::string mount_point;
};

struct drive_health {
    std::string identifier;
    drive_health_status status = drive_health_status::unknown;
    bool has_failure_predicted = false;
    bool failure_predicted = false;
    bool has_temperature_celsius = false;
    double temperature_celsius = 0.0;
    bool has_percent_used = false;
    std::uint32_t percent_used = 0U;
    bool has_available_spare_percent = false;
    std::uint32_t available_spare_percent = 0U;
    bool has_power_on_hours = false;
    std::uint64_t power_on_hours = 0U;
    bool has_power_cycles = false;
    std::uint64_t power_cycles = 0U;
    bool has_unsafe_shutdowns = false;
    std::uint64_t unsafe_shutdowns = 0U;
    bool has_media_errors = false;
    std::uint64_t media_errors = 0U;
    bool has_data_units_read_bytes = false;
    std::uint64_t data_units_read_bytes = 0U;
    bool has_data_units_written_bytes = false;
    std::uint64_t data_units_written_bytes = 0U;
};
```

### 函数接口

```cpp
result<std::vector<drive_entry>> drives();
result<std::vector<partition_entry>> partitions();
result<std::vector<partition_entry>> disk_partitions(std::string_view disk_identifier);
result<drive_health> health(std::string_view disk_identifier);
result<std::vector<drive_health>> all_drive_health();
```
