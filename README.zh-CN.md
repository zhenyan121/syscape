[English](README.md) | [简体中文](README.zh-CN.md)

# Syscape

Syscape 是一个现代化、零依赖、仅头文件（Header-Only）的 C++ 库，专用于查询平台、操作系统、底层硬件及执行环境信息。项目采用清晰的分层架构，覆盖从独立环境（Freestanding）与嵌入式目标（严格 C++11）到完整托管操作系统（Hosted，严格 C++17）的各类平台。

---

## 兼容性配置体系

Syscape 定义了严格的兼容性配置层级，确保受限资源目标不会引入托管环境依赖，同时为完整托管环境提供现代化、易用的查询接口。

| 兼容性配置 | 语言标准 | 核心特征 | 主要适用场景 |
| :--- | :--- | :--- | :--- |
| **独立环境最小化 (Freestanding Minimal)** | 严格 C++11 | 无堆内存分配、无异常、无 RTTI，编译期目标与工具链识别，调用方提供存储缓冲区。 | 嵌入式系统、实时操作系统 (RTOS)、裸机内核、引导加载程序 (Bootloader)。 |
| **托管完整版 (Hosted Full)** | 严格 C++17 | 值或错误语义 (`syscape::result<T>`)、UTF-8 编码字符串 (`std::string`)、全方位运行时系统自省。 | Linux、Windows 和 macOS 上的桌面、服务器、云原生与容器化应用。 |

---

## 核心设计原则

- **零外部依赖 (Zero Dependencies)**：Syscape 不依赖任何第三方库。所有平台查询均直接调用原生操作系统 API 和标准 C++ 设施。
- **仅头文件分发 (Header-Only)**：所有实现代码均位于 `.hpp` 头文件中，无需预先编译或链接预构建二进制库。
- **严格符合 ISO 语言标准**：采用严格标准 C++ 编写，不使用非标准编译器扩展、厂商私有 pragma 或编译器内建函数 (builtins)。
- **非抛出式错误处理 (Non-Throwing)**：托管完整版查询返回 `syscape::result<T>`，封装有效返回值或 `std::error_code`。预期的运行时查找失败与系统调用失败均不抛出异常。
- **真实的降级与回退语义 (Honest Fallback)**：对于缺失、不受支持或权限受限的信息，显式返回错误码（如 `syscape::errc::not_supported`）或显式 `unknown` 枚举值，绝不编造哨兵值或伪造默认数据。
- **全程 UTF-8 编码**：托管接口的所有文本数据均在平台边界统一转换为标准 UTF-8 编码。
- **单一定义规则安全 (ODR Safety) 与明确的生命周期**：所有头文件内的函数与变量定义均具备 ODR 安全性（采用 `inline`、`constexpr` 或模板）。系统底层原生资源均通过内部 RAII 包装器进行确定性释放。

---

## 公共模块概览

Syscape 将公共接口划分为 35 个聚焦特定领域的头文件，存放在 `include/syscape/` 目录下。项目不提供全量打包的大一统头文件。

### 1. 基础核心模块 (严格 C++11 / 独立环境)

| 头文件 | 兼容性配置 | 模块说明 |
| :--- | :--- | :--- |
| `<syscape/architecture.hpp>` | 独立环境 C++11 | 目标 CPU 架构族、数据模型（LP64、LLP64 等）与字节序。 |
| `<syscape/toolchain.hpp>` | 独立环境 C++11 | 编译器识别、版本三元组及标准库实现信息。 |
| `<syscape/execution_environment.hpp>` | 独立环境 C++11 | 执行运行时分类（托管、沙盒、RTOS、裸机、兼容层）。 |
| `<syscape/capability.hpp>` | 独立最小版 C++11 | 零动态分配的能力状态词汇表（`available`、`unsupported`、`permission_required`、`temporarily_unavailable`）。 |
| `<syscape/error.hpp>` | 托管 C++11 | 可移植的 `syscape::errc` 错误码枚举、错误类别及标准错误库集成。 |
| `<syscape/result.hpp>` | 托管完整版 C++17 | 值-或-错误响应容器 `syscape::result<T>` 与 `syscape::result<void>`。 |

### 2. 系统核心模块 (托管完整版 C++17)

| 头文件 | 说明 |
| :--- | :--- |
| `<syscape/os.hpp>` | 操作系统产品名称、版本、构建号、内核名称与版本、主机名、启动时间、运行时间及启动 UUID。 |
| `<syscape/cpu.hpp>` | CPU 厂商、型号、物理/逻辑核心数、封装数、频率限制、瞬时主频、缓存拓扑、指令集特性与使用率计数器。 |
| `<syscape/memory.hpp>` | 系统分页大小、物理内存总量、可用内存、交换分区/分页文件、提交配额、大页支持、内存压力 (PSI) 及负载百分比。 |
| `<syscape/process.hpp>` | 当前进程 ID、父进程 PID、可执行文件路径、命令行参数、工作目录、启动时间、CPU/内存开销、调度优先级、亲和性掩码及资源限制。 |
| `<syscape/process_list.hpp>` | 系统全量进程清单枚举与可观测元数据快照。 |
| `<syscape/user.hpp>` | 实际/有效用户 ID、组 ID、附加组清单、特权级别、登录名/用户名、用户主目录、登录 Shell 及活动会话。 |
| `<syscape/filesystem.hpp>` | 挂载文件系统表、文件系统类型、容量/可用空间统计及文件名最大长度。 |
| `<syscape/storage.hpp>` | 物理存储驱动器枚举、磁盘分区表及驱动器健康/SMART 诊断。 |

### 3. 网络与连接模块 (托管完整版 C++17)

| 头文件 | 说明 |
| :--- | :--- |
| `<syscape/network.hpp>` | 网络接口、MAC 地址、IPv4/IPv6 单播地址、IP 路由表、默认网关、DNS 解析器配置及流量统计。 |
| `<syscape/connection.hpp>` | 活动 TCP/UDP 套接字连接表、监听端点、进程关联归属 (PID/UID) 及内核缓冲区队列深度。 |
| `<syscape/wifi.hpp>` | Wi-Fi 主机适配器、无线电电源状态、连接 SSID/BSSID、信号强度 (RSSI/质量)、Wi-Fi 代数 (Wi-Fi 4/5/6/7) 及已保存网络配置。 |
| `<syscape/bluetooth.hpp>` | 蓝牙主机控制器、无线电状态、SIG 厂商标识、已配对设备表、已连接设备表及设备类别分类。 |

### 4. 硬件与外设模块 (托管完整版 C++17)

| 头文件 | 说明 |
| :--- | :--- |
| `<syscape/hardware.hpp>` | 系统制造商/型号/版本、主板信息、BIOS/固件版本与发布日期、机箱形态、硬件 UUID、PCI/USB 设备清单及物理内存插槽。 |
| `<syscape/gpu.hpp>` | 已安装 GPU/图形控制器设备、厂商分类、驱动版本及显存 (VRAM) 容量。 |
| `<syscape/display.hpp>` | 已连接显示器屏幕、虚拟桌面边界、工作区、显示分辨率模式、刷新率、DPI 缩放比例及屏幕旋转方向。 |
| `<syscape/power.hpp>` | 外部交流电源状态、电池清单、充放电状态、健康度、设计/完全容量、端子电压、功耗速率及剩余使用时间估算。 |
| `<syscape/sensor.hpp>` | 硬件温度传感器、散热风扇转速 (RPM) 及操作系统温区。 |
| `<syscape/audio.hpp>` | 音频端点（播放/采集）、系统默认音频设备、声道数量及采样率。 |
| `<syscape/camera.hpp>` | 摄像头与视频捕获设备、镜头朝向（前置/后置/外置）、总线连接及视频格式能力。 |
| `<syscape/input.hpp>` | 键盘、鼠标、触摸输入设备、游戏手柄及硬件总线连接类型。 |
| `<syscape/printer.hpp>` | 已安装打印队列、队列工作状态、系统默认打印机、连接类型及硬件功能特性。 |

### 5. 运行时、环境与安全模块 (托管完整版 C++17)

| 头文件 | 说明 |
| :--- | :--- |
| `<syscape/environment.hpp>` | 环境变量查询与枚举、可执行文件 PATH 搜索、当前工作目录及标准用户目录（主目录、临时目录、配置、数据、缓存）。 |
| `<syscape/resource.hpp>` | 系统平均负载 (1/5/15 分钟)、可运行调度实体数、系统进程总数、线程总数、打开文件数及文件描述符限制。 |
| `<syscape/security.hpp>` | UEFI 安全启动状态、TPM 版本与制造商、活动 Linux 安全模块 (LSM)、Linux 内核 Lockdown、ASLR 随机化模式、CPU 硬件漏洞缓解措施、进程权限能力及存储卷加密。 |
| `<syscape/virtualization.hpp>` | 虚拟机 Hypervisor 检测 (KVM、VMware、Hyper-V、Xen 等)、容器检测 (Docker、Podman、LXC、Kubernetes)、WSL 环境检测、应用沙盒检测及 cgroup/命名空间隔离自省。 |
| `<syscape/software.hpp>` | 系统服务与守护进程、已加载内核驱动/模块、已安装软件包与应用、待处理系统更新及已安装语言运行时。 |
| `<syscape/locale.hpp>` | 进程 Locale 标识、文本编码代码页、本地时区 UTC 偏移量、用户首选语言列表、国家/地区代码及本地时区标识符。 |
| `<syscape/numa.hpp>` | NUMA 拓扑支持、节点数量、CPU 亲和性掩码、各节点物理/空闲内存及节点间距离矩阵。 |
| `<syscape/ipc.hpp>` | System V 与 POSIX 进程间通信 (IPC) 共享内存段、消息队列、信号量集、本地 Unix 域套接字及内核 IPC 容量上限。 |

---

## 快速入门

### 1. 独立环境编译目标事实查询 (严格 C++11)

```cpp
#include <syscape/architecture.hpp>
#include <syscape/toolchain.hpp>
#include <syscape/execution_environment.hpp>

int main() {
    // 纯独立 C++11 目标查询，无托管运行时依赖、无堆内存分配、无 RTTI
    const syscape::architecture arch = syscape::target_architecture();
    const syscape::byte_order endian = syscape::target_byte_order();
    const syscape::compiler comp = syscape::target_compiler();
    const syscape::execution_environment env = syscape::target_execution_environment();

    // 在裸机或独立环境中安全可用的常量 C 字符串标签
    const char* arch_str = syscape::architecture_name(arch);
    const char* endian_str = syscape::byte_order_name(endian);
    const char* comp_str = syscape::compiler_name(comp);
    const char* env_str = syscape::execution_environment_name(env);

    (void)arch_str;
    (void)endian_str;
    (void)comp_str;
    (void)env_str;

    return (arch != syscape::architecture::unknown) ? 0 : 1;
}
```

### 2. 托管完整版系统与资源查询 (严格 C++17)

```cpp
#include <iostream>
#include <syscape/os.hpp>
#include <syscape/cpu.hpp>
#include <syscape/memory.hpp>

int main() {
    // 查询操作系统产品名称
    if (auto os_name = syscape::os::product_name()) {
        std::cout << "操作系统: " << *os_name << "\n";
    } else {
        std::cerr << "查询操作系统失败: " << os_name.error().message() << "\n";
    }

    // 查询 CPU 物理与逻辑核心数
    auto logical_cores = syscape::cpu::online_logical_processor_count();
    auto physical_cores = syscape::cpu::online_physical_core_count();
    if (logical_cores && physical_cores) {
        std::cout << "CPU 拓扑: " << *physical_cores << " 物理核心, "
                  << *logical_cores << " 逻辑处理器\n";
    }

    // 查询物理内存总量与可用量
    auto total_ram = syscape::memory::physical_memory_bytes();
    auto avail_ram = syscape::memory::available_memory_bytes();
    if (total_ram && avail_ram) {
        std::cout << "物理内存: 可用 " << (*avail_ram / (1024 * 1024)) << " MiB / 总计 "
                  << (*total_ram / (1024 * 1024)) << " MiB\n";
    }

    return 0;
}
```

---

## 构建集成

### 通过 CMake `FetchContent` 引入

```cmake
include(FetchContent)

FetchContent_Declare(
    syscape
    GIT_REPOSITORY https://github.com/zhenyan121/syscape.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(syscape)

# 针对独立环境最小化头文件 (C++11 要求):
target_link_libraries(my_embedded_target PRIVATE syscape::syscape)

# 针对托管完整版查询模块 (C++17 要求):
target_link_libraries(my_hosted_target PRIVATE syscape::hosted)
```

### 通过 CMake `add_subdirectory` 引入

```cmake
add_subdirectory(path/to/syscape)

# 链接对应配置目标
target_link_libraries(my_application PRIVATE syscape::hosted)
```

### 直接包含头文件

由于 Syscape 采用仅头文件且零外部依赖的设计，可以直接将 `include/` 目录复制或加入到项目的头文件搜索路径中，并将编译器标准设置为 C++17（使用独立环境最小化头文件时可为 C++11）。

---

## 测试与示例编译

在本地构建测试套件与示例程序：

```bash
# 生成构建目录与配置
cmake -B build -DSYSCAPE_BUILD_TESTS=ON -DSYSCAPE_BUILD_EXAMPLES=ON

# 编译所有目标
cmake --build build

# 运行全量单元测试
ctest --test-dir build --output-on-failure
```

---

## 文档与参考资料

- [API 参考文档索引](docs/api/README.zh-CN.md)
  - [基础核心 API 参考](docs/api/foundation.zh-CN.md)
  - [系统核心 API 参考](docs/api/system-core.zh-CN.md)
  - [网络与连接 API 参考](docs/api/network-connectivity.zh-CN.md)
  - [硬件与外设 API 参考](docs/api/hardware-peripherals.zh-CN.md)
  - [运行时、环境与安全 API 参考](docs/api/runtime-security.zh-CN.md)
- [支持矩阵与信息目录 (Support Matrix)](docs/support-matrix.md)
- [平台与架构支持目录 (Platform Catalog)](docs/platform-catalog.md)

---

## 开源协议

Syscape 遵循 [MIT 许可证](LICENSE) 开源。
