[English](README.md) | [简体中文](README.zh-CN.md)

# Syscape API 参考文档

本目录包含 Syscape 的正式 API 参考文档。

---

## 架构与设计规范

### 1. 兼容性配置体系

Syscape 的每个头文件均显式声明其最低兼容性配置要求与语言标准：

- **独立环境最小化 (Freestanding Minimal, `cxx_std_11`)**：该配置下的头文件仅需严格标准的 C++11 支持。完全不进行堆内存动态分配，不依赖 `std::string`、RTTI 或 C++ 异常，适用于裸机内核、RTOS 及引导加载程序等严苛环境。
- **托管完整版 (Hosted Full, `cxx_std_17`)**：该配置下的头文件需要严格标准的 C++17 及完整的托管 C++ 标准库支持。提供运行时平台查询接口，统一返回携带 UTF-8 编码 `std::string` 的 `syscape::result<T>`。

### 2. 错误处理模型

托管完整版查询操作通过 `<syscape/result.hpp>` 中定义的非抛出式 `syscape::result<T>` 类型报告运行时状态与预期的平台查询失败。

```cpp
#include <syscape/os.hpp>
#include <iostream>

void example() {
    syscape::result<std::string> name = syscape::os::product_name();
    if (name.has_value()) { // 或直接使用 if (name)
        std::cout << "操作系统产品: " << *name << "\n";
    } else {
        std::error_code ec = name.error();
        std::cerr << "查询失败: " << ec.message() << " (" << ec.value() << ")\n";
    }
}
```

#### 可移植错误条件 (`syscape::errc`)
当发生可移植层级的失败时，Syscape 将错误映射到 `<syscape/error.hpp>` 中定义的 `syscape::errc` 枚举：

| 错误枚举值 | 说明 |
| :--- | :--- |
| `errc::success` | 操作成功。 |
| `errc::unknown` | 未指定或未识别的错误。 |
| `errc::not_supported` | 目标平台不支持所请求的能力或信息。 |
| `errc::permission_denied` | 调用方缺乏执行查询所需的操作系统权限。 |
| `errc::not_found` | 未找到请求的实体、文件、属性或记录。 |
| `errc::temporarily_unavailable` | 信息暂时不可用（例如资源处于锁状态或读取期间发生大小竞争）。 |
| `errc::malformed_data` | 平台数据源或描述符包含格式错误或自相矛盾的数据。 |
| `errc::io_error` | 底层操作系统系统调用或 I/O 操作返回错误。 |
| `errc::invalid_encoding` | 平台文本数据无法验证或转换为有效的 UTF-8 编码。 |
| `errc::value_too_large` | 平台数值超出了目标表示类型的范围。 |
| `errc::resource_exhausted` | 系统资源（如文件描述符或内存）已耗尽。 |
| `errc::invalid_argument` | 传递给查询函数的参数无效。 |

在可能的情况下，Syscape 会优先保留底层原生的操作系统错误码（如 POSIX `errno`、Windows `GetLastError()`、Mach `kern_return_t`），以提供最精确的诊断信息。

### 3. 文本与字符串编码

托管完整版接口返回的所有文本数据均在平台边界转换为标准的 UTF-8 编码并存储于 `std::string` 中。在 Windows 平台上，宽字符字符串（`wchar_t` / UTF-16）会在系统调用边界安全转换。若遇到无法转换的损坏字节序列，接口将返回 `errc::invalid_encoding`，绝不返回乱码或进行静默字符替换。

### 4. 并发与线程安全

除个别底层操作系统 API 限制（见相关模块文档说明）外，Syscape 的所有查询函数均为无状态设计，支持多线程并发安全调用。

---

## API 参考模块索引

| 模块参考文档 | 包含的公共头文件 | 所属领域 |
| :--- | :--- | :--- |
| [**基础核心 API (Foundation)**](foundation.zh-CN.md) | `<syscape/architecture.hpp>`<br>`<syscape/toolchain.hpp>`<br>`<syscape/execution_environment.hpp>`<br>`<syscape/capability.hpp>`<br>`<syscape/error.hpp>`<br>`<syscape/result.hpp>` | 编译目标架构、编译器识别、标准库实现、运行环境分类、能力状态词汇表、错误码与结果容器。 |
| [**系统核心 API (System Core)**](system-core.zh-CN.md) | `<syscape/os.hpp>`<br>`<syscape/cpu.hpp>`<br>`<syscape/memory.hpp>`<br>`<syscape/process.hpp>`<br>`<syscape/process_list.hpp>`<br>`<syscape/user.hpp>`<br>`<syscape/filesystem.hpp>`<br>`<syscape/storage.hpp>` | 操作系统标识、CPU 拓扑与缓存、物理与虚拟内存、进程信息、用户会话、挂载文件系统与存储设备。 |
| [**网络与连接 API (Network & Connectivity)**](network-connectivity.zh-CN.md) | `<syscape/network.hpp>`<br>`<syscape/connection.hpp>`<br>`<syscape/wifi.hpp>`<br>`<syscape/bluetooth.hpp>` | 网络接口、路由表、DNS 配置、全系统监听端点与活动连接表、Wi-Fi 适配器及蓝牙无线电。 |
| [**硬件与外设 API (Hardware & Peripherals)**](hardware-peripherals.zh-CN.md) | `<syscape/hardware.hpp>`<br>`<syscape/gpu.hpp>`<br>`<syscape/display.hpp>`<br>`<syscape/power.hpp>`<br>`<syscape/sensor.hpp>`<br>`<syscape/audio.hpp>`<br>`<syscape/camera.hpp>`<br>`<syscape/input.hpp>`<br>`<syscape/printer.hpp>` | 主板、BIOS/UEFI 固件、GPU 显卡、显示器、电池电源、温度传感器、音频设备、摄像头、输入设备及打印机。 |
| [**运行时、安全与 IPC API (Runtime & Security)**](runtime-security.zh-CN.md) | `<syscape/environment.hpp>`<br>`<syscape/resource.hpp>`<br>`<syscape/security.hpp>`<br>`<syscape/virtualization.hpp>`<br>`<syscape/software.hpp>`<br>`<syscape/locale.hpp>`<br>`<syscape/numa.hpp>`<br>`<syscape/ipc.hpp>` | 环境变量、系统负载与限制、安全启动、虚拟化环境检测、已安装软件包与驱动服务、区域设置、NUMA 拓扑及进程间通信 (IPC)。 |
