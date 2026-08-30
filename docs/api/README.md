[English](README.md) | [简体中文](README.zh-CN.md)

# Syscape API Reference Documentation

This directory contains the formal API reference documentation for Syscape.

---

## Architectural and Design Conventions

### 1. Compatibility Profiles

Every Syscape header explicitly documents its minimum compatibility profile and language standard:

- **Freestanding Minimal (`cxx_std_11`)**: Headers in this profile require only strict standard C++11. They perform zero heap allocations, do not depend on `std::string`, RTTI, or exceptions, and are safe for bare-metal, RTOS, and bootloader environments.
- **Hosted Full (`cxx_std_17`)**: Headers in this profile require strict standard C++17 and the full hosted C++ standard library. They provide runtime platform query operations returning `syscape::result<T>` with UTF-8 `std::string` values.

### 2. Error Handling Model

Hosted Full queries report runtime status and expected platform lookup failures using the non-throwing `syscape::result<T>` type defined in `<syscape/result.hpp>`.

```cpp
#include <syscape/os.hpp>
#include <iostream>

void example() {
    syscape::result<std::string> name = syscape::os::product_name();
    if (name.has_value()) { // or if (name)
        std::cout << "Product: " << *name << "\n";
    } else {
        std::error_code ec = name.error();
        std::cerr << "Query failed: " << ec.message() << " (" << ec.value() << ")\n";
    }
}
```

#### Portable Error Conditions (`syscape::errc`)
When a portable failure occurs, Syscape maps the condition to the `syscape::errc` enumeration defined in `<syscape/error.hpp>`:

| Error Code | Meaning |
| :--- | :--- |
| `errc::success` | Operation succeeded. |
| `errc::unknown` | Unspecified or unrecognized failure. |
| `errc::not_supported` | Requested capability or information is not supported on the target platform. |
| `errc::permission_denied` | Caller lacks necessary OS privileges or permissions. |
| `errc::not_found` | Requested entity, file, property, or record was not found. |
| `errc::temporarily_unavailable` | Information is temporarily unavailable (e.g., resource locked or size race during read). |
| `errc::malformed_data` | Platform data source or descriptor contains unparseable or contradictory data. |
| `errc::io_error` | Underlying OS system call or I/O operation returned an error. |
| `errc::invalid_encoding` | Platform text could not be validated or converted to valid UTF-8. |
| `errc::value_too_large` | Platform value exceeded the range of the representation type. |
| `errc::resource_exhausted` | System resources (e.g., file descriptors or memory) were exhausted. |
| `errc::invalid_argument` | An invalid argument was passed to the query function. |

Native platform error codes (e.g., POSIX `errno`, Windows `GetLastError()`, Mach `kern_return_t`) are preserved whenever possible to retain diagnostic fidelity.

### 3. Text and String Encoding

All text returned by Hosted Full queries is normalized to UTF-8 in `std::string`. On Windows, wide-character strings (`wchar_t` / UTF-16) are converted to UTF-8 at the system API boundary. If invalid byte sequences are encountered that cannot be cleanly converted, `errc::invalid_encoding` is returned rather than returning corrupted or silent replacement text.

### 4. Concurrency and Thread Safety

All Syscape query functions are stateless and safe to call concurrently from multiple threads unless a specific underlying OS API constraint is explicitly noted in the module documentation.

---

## API Reference Modules

| Module Reference | Covered Public Headers | Domain |
| :--- | :--- | :--- |
| [**Foundation API**](foundation.md) | `<syscape/architecture.hpp>`<br>`<syscape/toolchain.hpp>`<br>`<syscape/execution_environment.hpp>`<br>`<syscape/capability.hpp>`<br>`<syscape/error.hpp>`<br>`<syscape/result.hpp>` | Target architectures, compilers, standard libraries, runtime environments, capability vocabulary, error codes, and result containers. |
| [**System Core API**](system-core.md) | `<syscape/os.hpp>`<br>`<syscape/cpu.hpp>`<br>`<syscape/memory.hpp>`<br>`<syscape/process.hpp>`<br>`<syscape/process_list.hpp>`<br>`<syscape/user.hpp>`<br>`<syscape/filesystem.hpp>`<br>`<syscape/storage.hpp>` | Operating system identity, CPU topology and caches, physical and virtual memory, processes, users, filesystems, and storage devices. |
| [**Network & Connectivity API**](network-connectivity.md) | `<syscape/network.hpp>`<br>`<syscape/connection.hpp>`<br>`<syscape/wifi.hpp>`<br>`<syscape/bluetooth.hpp>` | Network interfaces, routing, DNS, listening endpoints, active connections, Wi-Fi adapters, and Bluetooth radios. |
| [**Hardware & Peripherals API**](hardware-peripherals.md) | `<syscape/hardware.hpp>`<br>`<syscape/gpu.hpp>`<br>`<syscape/display.hpp>`<br>`<syscape/power.hpp>`<br>`<syscape/sensor.hpp>`<br>`<syscape/audio.hpp>`<br>`<syscape/camera.hpp>`<br>`<syscape/input.hpp>`<br>`<syscape/printer.hpp>` | Motherboards, BIOS/UEFI, GPUs, displays, batteries, thermal sensors, audio endpoints, cameras, input devices, and printers. |
| [**Runtime, Security & IPC API**](runtime-security.md) | `<syscape/environment.hpp>`<br>`<syscape/resource.hpp>`<br>`<syscape/security.hpp>`<br>`<syscape/virtualization.hpp>`<br>`<syscape/software.hpp>`<br>`<syscape/locale.hpp>`<br>`<syscape/numa.hpp>`<br>`<syscape/ipc.hpp>` | Environment variables, system load, Secure Boot, virtualization detection, installed packages/services, locale, NUMA topology, and IPC. |
