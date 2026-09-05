[English](README.md) | [简体中文](README.zh-CN.md)

# Syscape

Syscape is a modern, zero-dependency, header-only C++ library designed for querying platform, operating system, hardware, and execution environment information. It provides an explicit layered architecture spanning from freestanding and embedded targets (strict C++11) to full hosted operating systems (strict C++17).

---

## Architectural Profiles

Syscape defines strict compatibility profiles to ensure that constrained targets are never burdened by hosted dependencies, while hosted environments receive a modern and expressive query API.

| Profile | Standard | Characteristics | Primary Use Cases |
| :--- | :--- | :--- | :--- |
| **Freestanding Minimal** | Strict C++11 | Allocation-free, no exceptions, no RTTI, compile-time target and toolchain identification, caller-provided buffers. | Embedded systems, RTOS, bare-metal kernels, bootloaders. |
| **Hosted Full** | Strict C++17 | Value-or-error semantics (`syscape::result<T>`), UTF-8 encoded text (`std::string`), comprehensive runtime introspection. | Desktop, server, cloud, and containerized applications on Linux, Windows, and macOS. |

---

## Key Design Principles

- **Zero Dependencies**: Syscape does not depend on any third-party libraries. All platform queries use native operating system APIs and standard C++ facilities.
- **Header-Only Distribution**: Implementation code resides entirely within header files (`.hpp`). No separate compilation or pre-built binaries are required.
- **Strict ISO Standard Compliance**: Written in strict standard C++ without non-standard compiler extensions, vendor pragmas, or compiler builtins.
- **Non-Throwing Error Handling**: Hosted Full queries return `syscape::result<T>`, encapsulating either a valid value or a `std::error_code`. Runtime lookup and platform failures do not throw exceptions.
- **Honest Fallback Semantics**: Missing, unsupported, or restricted information returns explicit error codes (such as `syscape::errc::not_supported`) or explicit `unknown` enum values. Syscape never returns fabricated sentinels or synthetic default values.
- **UTF-8 Everywhere**: All textual data across hosted interfaces is normalized to UTF-8 at the platform boundary.
- **ODR Safety & Clean Lifecycles**: All functions and variables are ODR-safe (`inline`, `constexpr`, or templated). Native resources are managed via deterministic internal RAII wrappers.

---

## Public Modules Overview

Syscape organizes its public interface into 35 focused domain headers located under `include/syscape/`. Monolithic umbrella headers are deliberately not provided.

### 1. Foundation Modules (Strict C++11 / Freestanding)

| Header | Compatibility | Description |
| :--- | :--- | :--- |
| `<syscape/architecture.hpp>` | Freestanding C++11 | Target architecture family, data model (LP64, LLP64, etc.), and byte order. |
| `<syscape/toolchain.hpp>` | Freestanding C++11 | Compiler identification, version triples, and standard-library implementation facts. |
| `<syscape/execution_environment.hpp>` | Freestanding C++11 | Execution runtime classification (hosted, sandboxed, RTOS, bare-metal, compatibility). |
| `<syscape/capability.hpp>` | Freestanding C++11 | Allocation-free vocabulary for capability states (`available`, `unsupported`, `permission_required`, `temporarily_unavailable`). |
| `<syscape/error.hpp>` | Hosted C++11 | Portable `syscape::errc` enum, error categories, and standard error integration. |
| `<syscape/result.hpp>` | Hosted Full C++17 | Value-or-error container `syscape::result<T>` and `syscape::result<void>`. |

### 2. System Core Modules (Hosted Full C++17)

| Header | Description |
| :--- | :--- |
| `<syscape/os.hpp>` | OS product name, version, build, kernel name/version, hostname, boot time, uptime, and boot ID. |
| `<syscape/cpu.hpp>` | Vendor, model, physical/logical core counts, package count, frequency bounds, instantaneous clocks, cache topology, instruction-set features, and utilization counters. |
| `<syscape/memory.hpp>` | Page size, physical memory, available memory, swap/pagefile, commit accounting, huge pages, memory pressure (PSI), and load percentage. |
| `<syscape/process.hpp>` | Current process ID, parent PID, executable path, command line, working directory, start time, CPU/memory usage, scheduling priority, affinity mask, and resource limits. |
| `<syscape/process_list.hpp>` | System-wide process enumeration and observable metadata snapshots. |
| `<syscape/user.hpp>` | Real/effective user IDs, group IDs, supplementary groups, privilege level, login/user names, home directory, shell, and active sessions. |
| `<syscape/filesystem.hpp>` | Mount table enumeration, filesystem types, capacity/space statistics, and maximum component lengths. |
| `<syscape/storage.hpp>` | Physical storage drive enumeration, disk partitions, and drive health/SMART diagnostics. |

### 3. Network and Connectivity Modules (Hosted Full C++17)

| Header | Description |
| :--- | :--- |
| `<syscape/network.hpp>` | Network interfaces, MAC addresses, IPv4/IPv6 unicast addresses, IP routes, default gateways, DNS resolver configuration, and traffic statistics. |
| `<syscape/connection.hpp>` | Active TCP/UDP socket connections, listening endpoints, process ownership (PID/UID), and kernel buffer queues. |
| `<syscape/wifi.hpp>` | Wi-Fi host adapters, radio power states, connected SSID/BSSID, signal strength (RSSI/quality), Wi-Fi generations (Wi-Fi 4/5/6/7), and saved network profiles. |
| `<syscape/bluetooth.hpp>` | Bluetooth host controllers, radio state, SIG manufacturer IDs, paired devices, connected devices, and device classes. |

### 4. Hardware and Peripheral Modules (Hosted Full C++17)

| Header | Description |
| :--- | :--- |
| `<syscape/hardware.hpp>` | System manufacturer/model/version, motherboard info, BIOS/firmware version and date, chassis form factor, hardware UUID, PCI device inventory, USB device inventory, and physical memory modules. |
| `<syscape/gpu.hpp>` | Installed GPU/graphics devices, vendor classification, driver versions, and dedicated VRAM capacity. |
| `<syscape/display.hpp>` | Connected display monitors, desktop bounds, work areas, display modes, refresh rates, DPI scale factors, and orientation. |
| `<syscape/power.hpp>` | AC power status, batteries, charge/health percentages, design/full energy, voltage, power rate, and time remaining. |
| `<syscape/sensor.hpp>` | Hardware temperature sensors, rotational fan speed sensors (RPM), and thermal zones. |
| `<syscape/audio.hpp>` | Audio endpoints (playback/capture), default audio devices, channel counts, and sample rates. |
| `<syscape/camera.hpp>` | Video capture devices/webcams, facing orientation (front/back/external), transport bus, and video format capabilities. |
| `<syscape/input.hpp>` | Keyboards, mice, touch devices, gamepads, and hardware bus classifications. |
| `<syscape/printer.hpp>` | Installed print queues, queue status, default printer, connection types, and hardware capabilities. |

### 5. Runtime, Environment, and Security Modules (Hosted Full C++17)

| Header | Description |
| :--- | :--- |
| `<syscape/environment.hpp>` | Environment variables, executable PATH lookup, current working directory, and standard directories (home, temp, config, data, cache). |
| `<syscape/resource.hpp>` | System load averages (1/5/15 min), runnable scheduling entities, total process count, thread count, open file count, and file descriptor limits. |
| `<syscape/security.hpp>` | UEFI Secure Boot status, TPM version/manufacturer, active Linux Security Modules (LSM), Linux kernel lockdown, ASLR mode, CPU hardware vulnerability mitigations, process capabilities, and volume encryption. |
| `<syscape/virtualization.hpp>` | Hypervisor detection (KVM, VMware, Hyper-V, Xen, etc.), container detection (Docker, Podman, LXC, Kubernetes), WSL detection, application sandbox detection, and cgroup/namespace inspection. |
| `<syscape/software.hpp>` | System services/daemons, loaded kernel drivers/modules, installed software packages, pending system updates, and installed language runtimes. |
| `<syscape/locale.hpp>` | Process locale identifier, text encoding codeset, UTC offset, user preferred languages, country/region code, and local time-zone identifier. |
| `<syscape/numa.hpp>` | NUMA availability, node counts, CPU affinities, per-node physical/free memory, and distance matrix. |
| `<syscape/ipc.hpp>` | System V and POSIX IPC shared memory segments, message queues, semaphore sets, local domain sockets, and kernel IPC limits. |

---

## Quick Start

### 1. Freestanding Minimal Target Facts (Strict C++11)

```cpp
#include <syscape/architecture.hpp>
#include <syscape/toolchain.hpp>
#include <syscape/execution_environment.hpp>

int main() {
    // Pure freestanding C++11 target queries without hosted runtime, heap allocation, or RTTI
    const syscape::architecture arch = syscape::target_architecture();
    const syscape::byte_order endian = syscape::target_byte_order();
    const syscape::compiler comp = syscape::target_compiler();
    const syscape::execution_environment env = syscape::target_execution_environment();

    // Constant C-string labels guaranteed safe in freestanding environments
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

### 2. Hosted Full System and Resource Query (Strict C++17)

```cpp
#include <iostream>
#include <syscape/os.hpp>
#include <syscape/cpu.hpp>
#include <syscape/memory.hpp>

int main() {
    // Query Operating System Product Name
    if (auto os_name = syscape::os::product_name()) {
        std::cout << "OS Product: " << *os_name << "\n";
    } else {
        std::cerr << "Failed to get OS name: " << os_name.error().message() << "\n";
    }

    // Query CPU Physical and Logical Core Counts
    auto logical_cores = syscape::cpu::online_logical_processor_count();
    auto physical_cores = syscape::cpu::online_physical_core_count();
    if (logical_cores && physical_cores) {
        std::cout << "CPU Topology: " << *physical_cores << " physical cores, "
                  << *logical_cores << " logical processors\n";
    }

    // Query System Physical Memory
    auto total_ram = syscape::memory::physical_memory_bytes();
    auto avail_ram = syscape::memory::available_memory_bytes();
    if (total_ram && avail_ram) {
        std::cout << "Physical Memory: " << (*avail_ram / (1024 * 1024)) << " MiB available / "
                  << (*total_ram / (1024 * 1024)) << " MiB total\n";
    }

    return 0;
}
```

---

## Integration

### Using CMake `FetchContent`

```cmake
include(FetchContent)

FetchContent_Declare(
    syscape
    GIT_REPOSITORY https://github.com/zhenyan121/syscape.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(syscape)

# For Freestanding Minimal headers (C++11 requirement):
target_link_libraries(my_embedded_target PRIVATE syscape::syscape)

# For Hosted Full query modules (C++17 requirement):
target_link_libraries(my_hosted_target PRIVATE syscape::hosted)
```

### Using CMake `add_subdirectory`

```cmake
add_subdirectory(path/to/syscape)

# Link the appropriate target
target_link_libraries(my_application PRIVATE syscape::hosted)
```

### Direct Header Inclusion

Because Syscape is header-only with zero dependencies, you can also copy the `include/` directory directly into your project's include path and configure your compiler for C++17 (or C++11 for minimal profile headers).

On HP-UX, direct-header users must additionally define
`_XOPEN_SOURCE_EXTENDED=1`, `_PSTAT64=1`, and `_ICOD_BASE_INFO=1` on the
compiler command line. They must be defined before any system header is
included. `_PSTAT64=1` allows 32-bit applications to receive the full-width
counters exported by a 64-bit kernel, while `_ICOD_BASE_INFO=1` exposes the
processor-state fields used to exclude disabled processors. The exported
CMake targets and pkg-config metadata supply all three definitions
automatically.

---

## Building Tests and Examples

To build the test suite and example programs locally:

```bash
# Configure the build directory
cmake -B build -DSYSCAPE_BUILD_TESTS=ON -DSYSCAPE_BUILD_EXAMPLES=ON

# Compile the targets
cmake --build build

# Execute all tests
ctest --test-dir build --output-on-failure
```

---

## Documentation and References

- [API Reference Index](docs/api/README.md)
  - [Foundation API Reference](docs/api/foundation.md)
  - [System Core API Reference](docs/api/system-core.md)
  - [Network and Connectivity API Reference](docs/api/network-connectivity.md)
  - [Hardware and Peripherals API Reference](docs/api/hardware-peripherals.md)
  - [Runtime, Environment, and Security API Reference](docs/api/runtime-security.md)
- [Support Matrix and Information Catalog](docs/support-matrix.md)
- [Platform and Architecture Catalog](docs/platform-catalog.md)

---

## License

Syscape is licensed under the [MIT License](LICENSE).
