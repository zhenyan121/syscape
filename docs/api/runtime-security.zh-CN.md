[English](runtime-security.md) | [简体中文](runtime-security.zh-CN.md)

# 运行时、环境与安全 API 参考 (Runtime & Security)

运行时、环境与安全头文件提供进程环境变量与标准目录、系统容量与负载限制、内核安全特性、虚拟化与沙盒检测、系统软件与驱动服务清单、区域设置、NUMA 拓扑以及进程间通信 (IPC) 资源的深度查询接口。

本组所有头文件均要求 **托管完整版 (Hosted Full)** 配置与 **严格 C++17** 标准。

---

## 1. 进程环境与标准路径 (`<syscape/environment.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::environment`

### 类型与结构体

```cpp
struct environment_variable {
    std::string name;
    std::string value;
};
```

### 函数接口

```cpp
result<std::string> get(std::string_view name);
result<bool> has(std::string_view name);
result<std::vector<environment_variable>> environment_variables();
result<std::string> current_working_directory();
result<std::string> find_executable(std::string_view name);

constexpr char path_list_separator() noexcept;
constexpr char directory_separator() noexcept;

result<std::string> temp_directory();
result<std::string> home_directory();
result<std::string> config_directory();
result<std::string> data_directory();
result<std::string> cache_directory();

result<bool> is_interactive_stdin();
result<bool> is_interactive_stdout();
result<bool> is_interactive_stderr();
```

---

## 2. 系统资源与负载 (`<syscape/resource.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::resource`

### 类型与结构体

```cpp
struct load_snapshot {
    double one_minute;
    double five_minute;
    double fifteen_minute;
};

struct scheduling_snapshot {
    std::uint64_t runnable_entities;
    std::uint64_t total_entities;
};
```

### 函数接口

```cpp
result<load_snapshot> load_average();
result<scheduling_snapshot> scheduler_entities();
result<std::uint64_t> process_count();
result<std::uint64_t> thread_count();
result<std::uint64_t> open_file_count();
result<std::uint64_t> open_handle_count();
result<std::uint64_t> file_descriptor_limit();
```

---

## 3. 系统安全与完整性 (`<syscape/security.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::security`

### 类型与枚举

```cpp
enum class secure_boot_state : std::uint8_t {
    unknown, enabled, disabled, audit
};

enum class tpm_version : std::uint8_t {
    unknown, none, v1_2, v2_0, other
};

enum class lockdown_mode : std::uint8_t {
    unknown, none, integrity, confidentiality
};

enum class aslr_mode : std::uint8_t {
    unknown, disabled, partial, full
};

enum class mitigation_status : std::uint8_t {
    unknown, not_affected, mitigated, vulnerable
};

struct tpm_info {
    bool is_present = false;
    tpm_version version = tpm_version::unknown;
    std::optional<std::string> manufacturer;
    std::optional<std::string> firmware_version;
};

struct cpu_vulnerability_entry {
    std::string name;
    mitigation_status status = mitigation_status::unknown;
    std::string mitigation;
};

struct process_capabilities {
    std::uint64_t effective = 0U;
    std::uint64_t permitted = 0U;
    std::uint64_t inheritable = 0U;
    std::uint64_t bounding = 0U;
    std::uint64_t ambient = 0U;
};

struct privilege_entry {
    std::string name;
    bool is_enabled = false;
};

struct volume_encryption_info {
    std::string path_or_device;
    bool is_encrypted = false;
    std::optional<std::string> protection_status;
    std::optional<std::string> encryption_method;
};
```

### 函数接口

```cpp
result<secure_boot_state> secure_boot();
result<bool> is_secure_boot_enabled();
result<tpm_info> tpm();
result<std::vector<std::string>> security_modules();
result<lockdown_mode> lockdown();
result<bool> is_sip_enabled();
result<aslr_mode> aslr();
result<std::vector<cpu_vulnerability_entry>> cpu_vulnerabilities();
result<process_capabilities> capabilities();
result<std::vector<privilege_entry>> privileges();
result<volume_encryption_info> volume_encryption(std::string_view path);
result<std::vector<volume_encryption_info>> encrypted_volumes();
```

---

## 4. 虚拟化与容器检测 (`<syscape/virtualization.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::virtualization`

### 类型与枚举

```cpp
enum class hypervisor_vendor : std::uint8_t {
    unknown, none, kvm, qemu, vmware, virtualbox, hyper_v, xen,
    bhyve, parallels, apple_hypervisor, acrn, qnx_hypervisor, other
};

enum class container_runtime : std::uint8_t {
    unknown, none, docker, podman, lxc, lxd, containerd, kubernetes,
    systemd_nspawn, openvz, wsl, appbox, other
};

enum class sandbox_type : std::uint8_t {
    unknown, none, flatpak, snap, app_sandbox, app_container, other
};

enum class cgroup_version : std::uint8_t {
    unknown, none, v1, v2, hybrid
};

struct cgroup_info {
    cgroup_version version = cgroup_version::none;
    std::string path;
    std::vector<std::string> controllers;
    std::optional<std::uint64_t> memory_limit_bytes;
    std::optional<std::uint64_t> pids_limit;
};

struct namespace_info {
    std::string name;
    std::uint64_t inode = 0U;
    bool is_isolated = false;
};
```

### 函数接口

```cpp
result<bool> is_hypervisor_present();
result<hypervisor_vendor> hypervisor();
result<std::string> hypervisor_name();

result<bool> is_container();
result<container_runtime> container();
result<std::string> container_name();

result<bool> is_wsl();
result<std::uint32_t> wsl_version();

result<bool> is_sandboxed();
result<sandbox_type> sandbox();

result<cgroup_version> cgroup_hierarchy_version();
result<cgroup_info> current_cgroup();
result<std::vector<namespace_info>> namespaces();
result<bool> is_namespace_isolated();
```

---

## 5. 系统服务、驱动与软件包 (`<syscape/software.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::software`

### 类型与结构体

```cpp
struct service_entry {
    std::string name;
    std::string display_name;
    std::string state;
    std::string startup_type;
};

struct driver_entry {
    std::string name;
    std::string description;
    std::string version;
    std::string provider;
};

struct package_entry {
    std::string name;
    std::string version;
    std::string architecture;
    std::string publisher;
};

struct update_entry {
    std::string identifier;
    std::string title;
    std::string severity;
    bool is_installed = false;
};

struct runtime_entry {
    std::string name;
    std::string version;
    std::string install_path;
};
```

### 函数接口

```cpp
result<std::vector<service_entry>> services();
result<service_entry> find_service(std::string_view name);
result<std::vector<driver_entry>> loaded_drivers();
result<driver_entry> find_driver(std::string_view name);
result<std::vector<package_entry>> installed_packages();
result<package_entry> find_package(std::string_view name);
result<std::vector<update_entry>> system_updates();
result<std::vector<runtime_entry>> installed_runtimes();
```

---

## 6. 区域设置与偏好 (`<syscape/locale.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::locale`

### 函数接口

```cpp
result<std::string> current_locale();
result<std::string> text_encoding();
result<std::int32_t> utc_offset_seconds();
result<std::vector<std::string>> preferred_languages();
result<std::string> country_region_code();
result<std::string> time_zone_identifier();
```

---

## 7. NUMA 拓扑架构 (`<syscape/numa.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::numa`

### 类型与结构体

```cpp
struct numa_node {
    std::uint32_t id = 0U;
    std::vector<std::uint32_t> cpu_indices;
    std::optional<std::uint64_t> total_memory_bytes;
    std::optional<std::uint64_t> free_memory_bytes;
    std::vector<std::uint32_t> distances;
};
```

### 函数接口

```cpp
result<bool> is_numa_available();
result<std::uint32_t> node_count();
result<std::vector<numa_node>> nodes();
result<numa_node> node(std::uint32_t id);
result<std::uint32_t> current_thread_node();
```

---

## 8. 进程间通信 (IPC) (`<syscape/ipc.hpp>`)

- **兼容性配置**: 托管完整版 (C++17)
- **所属命名空间**: `namespace syscape::ipc`

### 类型与结构体

```cpp
struct shared_memory_segment {
    std::int32_t id = 0;
    std::uint32_t key = 0U;
    std::uint64_t size_bytes = 0U;
    std::uint32_t attach_count = 0U;
    std::uint32_t creator_uid = 0U;
};

struct message_queue {
    std::int32_t id = 0;
    std::uint32_t key = 0U;
    std::uint64_t current_messages = 0U;
    std::uint64_t max_bytes = 0U;
};

struct semaphore_set {
    std::int32_t id = 0;
    std::uint32_t key = 0U;
    std::uint32_t semaphore_count = 0U;
};

struct local_socket {
    std::string path;
    std::string protocol;
    std::uint32_t inode = 0U;
};

struct ipc_limits {
    std::optional<std::uint64_t> max_shared_memory_bytes;
    std::optional<std::uint64_t> max_shared_memory_segments;
    std::optional<std::uint64_t> max_message_queue_bytes;
    std::optional<std::uint64_t> max_message_queues;
    std::optional<std::uint64_t> max_semaphores;
};
```

### 函数接口

```cpp
result<std::vector<shared_memory_segment>> shared_memory_segments();
result<std::vector<message_queue>> message_queues();
result<std::vector<semaphore_set>> semaphore_sets();
result<std::vector<local_socket>> local_sockets();
result<ipc_limits> limits();
```
