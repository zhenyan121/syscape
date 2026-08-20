# Syscape Support Matrix and Information Catalog

Last reviewed: 2026-08-20

## Purpose

This document is the source of truth for the information Syscape intends to
expose and the support that has actually been implemented and verified. The
broader inventory of operating systems, compatibility environments,
architectures, toolchains, RTOS families, MCU families, WebAssembly hosts, and
SDK-restricted platforms is maintained in
[platform-catalog.md](platform-catalog.md).

Syscape uses layered C++17 compatibility profiles. A platform does not need to
expose every item in this catalog, and a minimal target does not need to compile
headers declared for a larger profile. Every header must compile on an unknown
target that satisfies its declared profile. An unavailable Hosted Full query
returns an error such as `syscape::errc::not_supported`; an unrecognized
Freestanding Minimal compile target reports an explicit `unknown` enum value.
Syscape never invents a value to make a platform appear supported.

The catalog is intentionally broader than the first implementation milestones.
New information categories may be added when a documented in-process system
API or platform data source is identified.

## Status Vocabulary

The status of a module and the status of a platform backend are tracked
separately.

| Status | Meaning |
| --- | --- |
| Not started | No public API or backend has been implemented. |
| Designing | Portable semantics and API are being specified. |
| In progress | Code exists but the required tests or documentation are incomplete. |
| Compiles | The declared headers compile and link for a concrete target, but runtime behavior has not been verified there. |
| Implemented | The portable API, fallback, and at least one backend are complete. |
| Verified | The backend has passed the required tests on the named platform. |
| SDK restricted | The target is cataloged, but implementation and verification require lawful access to a proprietary SDK and hardware. |
| Conditional | The platform can expose the information only with particular hardware, permissions, entitlements, or runtime facilities. |
| Not available | The platform does not expose the information through an acceptable source. |

An item must not be marked **Verified** based only on successful compilation,
cross-compilation, emulation, or similarity to another operating system.

Items remain **Not started** unless their rows or the verification evidence
below explicitly record a later state. A module-level state does not imply that
every cataloged platform backend has the same state.

## Compatibility Profiles

| Profile | Minimum contract | Error and text model |
| --- | --- | --- |
| Hosted Full | Complete hosted C++17 standard library and the full portable query API | `syscape::result<T>`, `std::error_code`, and UTF-8 `std::string` |
| Sandboxed/Restricted | Hosted or hosted-like C++17 with only publicly permitted information | Hosted error and text model; unavailable or denied data remains an explicit error |
| RTOS/Constrained | Per-header subset using official RTOS APIs and explicit board providers | Declared by each header according to available standard-library facilities |
| Freestanding Minimal | Allocation-free target, architecture, byte-order, toolchain, execution-environment, and board-capability facts | Enums, scalar values, compile-time constants, and caller-owned storage; no required `std::string` or `std::error_code` |
| SDK Restricted | Catalog entry only until lawful SDK access and permitted hardware verification exist | No API promise |

Profiles describe API availability, while implementation statuses describe
project progress. A target can be cataloged for Freestanding Minimal while all
of its Syscape modules remain Not started.

## Platform Roadmap

Platform tiers define implementation order, not API quality. Once a backend is
claimed as supported, it must follow the same error, encoding, testing, and
documentation rules as every other backend.

### Foundation track: Hosted and freestanding foundations

Before platform backends, Syscape will design both the Hosted Full foundation
and the independent Freestanding Minimal compile-target foundation. The
minimal foundation must not include or depend on the hosted error, string, or
runtime-query layer.

### Tier 1: Primary desktop and server platforms

| Platform family | Initial variants | Intended platform sources | Current status |
| --- | --- | --- | --- |
| Linux | glibc and musl; x86, Arm, and RISC-V | POSIX APIs, Linux system calls, procfs, sysfs, netlink, and documented kernel interfaces | In progress; OS module verified on one host |
| Windows | Supported Windows client and server releases; x86, x64, and Arm64 | Win32, NT-supported public APIs, IP Helper, SetupAPI, registry APIs, and other Windows SDK facilities | In progress; OS backend implemented but unverified |
| macOS | Supported macOS releases; x86-64 and Apple silicon | POSIX, `sysctl`, IOKit, SystemConfiguration, and other public system frameworks callable from C++ | In progress; OS backend implemented but unverified |

### Tier 2: BSD and mobile platforms

| Platform family | Initial variants | Intended platform sources | Current status |
| --- | --- | --- | --- |
| Android | Android NDK on Arm, Arm64, x86, and x86-64 | NDK APIs, Linux interfaces available to applications, and documented Android system data | Not started |
| FreeBSD | Supported releases and architectures available to the project | POSIX, `sysctl`, devstat, and documented FreeBSD interfaces | Not started |
| OpenBSD | Supported releases and architectures available to the project | POSIX, `sysctl`, and documented OpenBSD interfaces | Not started |
| NetBSD | Supported releases and architectures available to the project | POSIX, `sysctl`, and documented NetBSD interfaces | Not started |
| DragonFly BSD | Supported releases and architectures available to the project | POSIX, `sysctl`, and documented DragonFly interfaces | Not started |
| Apple mobile platforms | iOS, iPadOS, tvOS, watchOS, and visionOS | Public APIs permitted by the application sandbox | Not started |
| HarmonyOS and OpenHarmony | Public native C++ environments on supported products | Public system APIs and product-specific sandbox facilities | Not started |

### Tier 3: Other hosted, compatibility, and sandboxed platforms

| Platform family | Intended scope | Current status |
| --- | --- | --- |
| illumos and Solaris | Hosted C++17 environments using POSIX and documented native interfaces | Not started |
| AIX | Hosted C++17 environments using POSIX and documented AIX interfaces | Not started |
| HP-UX | C++17-capable releases and toolchains using documented HP-UX interfaces | Not started |
| Haiku | Hosted C++17 applications using POSIX and public Haiku APIs | Not started |
| Fuchsia | Hosted C++17 components using public Fuchsia SDK interfaces | Not started |
| SerenityOS, Redox OS, and GNU/Hurd | Public hosted interfaces available to each system | Not started |
| Cygwin, MinGW, MSYS2, WSL, and Wine | Backend selected by the produced executable's actual runtime, with compatibility-environment detection | Not started |
| WebAssembly and WASI | WASI runtimes and browser toolchains; most host information is expected to be sandboxed or unavailable | Not started |

### Tier 4: RTOS, bare-metal, and legacy targets

| Platform group | Intended scope | Current status |
| --- | --- | --- |
| QNX Neutrino and VxWorks | Hosted or constrained modules using documented native and POSIX APIs | Not started |
| FreeRTOS, Zephyr, RTEMS, ThreadX, NuttX, and other cataloged RTOS families | Selective runtime modules through official APIs and explicit board providers | Not started |
| Arduino, AVR, ESP, STM32, RP-series, Nordic, TI, NXP, Renesas, Microchip, RISC-V MCU, GD32, CH32, and BL-series families | Freestanding Minimal plus board-provider capabilities | Not started |
| DOS, FreeDOS, OS/2, ArcaOS, and other legacy environments | The largest profile supported by a strict C++17 toolchain and available runtime | Not started |

### SDK-restricted targets

PlayStation, Xbox, and Nintendo console families are cataloged in
[platform-catalog.md](platform-catalog.md) as **SDK restricted**. They are not
implementation commitments and cannot advance until the repository has lawful
SDK access and verification permitted by the applicable platform terms.

### Profile-specific fallback

Every public module must provide a fallback within its declared profile. A
Hosted Full fallback returns `not_supported` for information that portable
C++17 cannot expose. A Freestanding Minimal fallback reports explicit unknown
compile-target values and must remain allocation-free. A fallback does not
make headers from a larger profile available to a smaller profile.

## Planned Information Modules

The header names below are planned organization points, not existing API. A
module may be divided into additional focused headers during API design. There
will be no all-modules umbrella header.

| Priority | Planned header or domain | Information to expose | Current status |
| --- | --- | --- | --- |
| Foundation | `architecture.hpp` | Freestanding-safe target architecture family, data model, pointer width, and byte order with explicit unknown values | Implemented |
| Foundation | `toolchain.hpp` | Freestanding-safe compiler, language mode, and standard-library availability facts | Implemented |
| Foundation | `execution_environment.hpp` | Freestanding-safe hosted, sandboxed, RTOS, bare-metal, compatibility, and unknown environment classification | Implemented |
| Foundation | `error.hpp` | `syscape::errc`, error category integration, and portable error conditions | Implemented |
| Foundation | `result.hpp` | Non-throwing `syscape::result<T>` and `result<void>` value-or-error types | Implemented |
| Foundation | `capability.hpp` | Allocation-free capability state vocabulary for runtime and compile-time modules | Implemented |
| 1 | `os.hpp` | Product name, version, build, kernel name and version, host name, boot time, uptime, and boot identifier where available | Implemented; Linux verified, Windows and macOS unverified |
| 1 | `cpu.hpp` | Architecture, vendor, model, packages, physical and logical cores, topology, caches, instruction-set features, frequency, affinity, and utilization | Not started |
| 1 | `memory.hpp` | Physical memory, available memory, committed memory, swap or pagefile, page size, huge pages, pressure, and system utilization | Not started |
| 1 | `process.hpp` | Current process identity, parent, executable, command line, working directory, start time, CPU time, memory use, priority, affinity, threads, and resource limits | Not started |
| 1 | `user.hpp` | Current user identity, numeric or textual IDs, groups, home directory, shell, elevation, and login session | Not started |
| 1 | `filesystem.hpp` | Mounts, volumes, filesystem type, capacity, free space, block size, read-only state, and path limits | Not started |
| 1 | `network.hpp` | Network interfaces, addresses, prefix lengths, MAC addresses, MTU, state, routes, default gateways, DNS configuration, and host/domain names | Not started |
| 1 | `locale.hpp` | Locale, preferred languages, country or region, text encoding, time zone, and UTC offset | Not started |
| 2 | `storage.hpp` | Physical drives, partitions, bus and media type, model, firmware, capacity, logical and physical sector sizes, rotational state, removable state, and health data exposed by the OS | Not started |
| 2 | `power.hpp` | Batteries, charge, health, charging state, power source, estimated remaining time, and system power capabilities | Not started |
| 2 | `hardware.hpp` | System manufacturer and model, chassis, motherboard, firmware or BIOS, hardware UUID, and documented device inventory | Not started |
| 2 | `display.hpp` | Displays, bounds, resolution, work area, refresh rate, scale, orientation, color depth, and connection state | Not started |
| 2 | `gpu.hpp` | GPU name, vendor, device identity, driver, memory values exposed by the OS, and active adapter state | Not started |
| 2 | `virtualization.hpp` | Hypervisor presence and vendor, virtual machine hints, containers, namespaces, cgroups, jails, zones, WSL, and application sandboxing | Not started |
| 2 | `environment.hpp` | Process environment snapshot, temporary and configuration directories, terminal presence, and runtime environment characteristics | Not started |
| 2 | `resource.hpp` | System load, scheduler information, system-wide process and thread counts, handle or file descriptor limits, and other capacity limits | Not started |
| 3 | `security.hpp` | Secure Boot state, TPM presence, privilege state, filesystem encryption visibility, integrity facilities, and security capabilities publicly exposed by the OS | Not started |
| 3 | `sensor.hpp` | Thermal zones, temperatures, fan speeds, and other sensors available through documented system interfaces | Not started |
| 3 | `audio.hpp` | Audio input and output devices, default devices, capabilities, and connection state | Not started |
| 3 | `input.hpp` | Keyboards, pointing devices, touch, game controllers, and other input devices exposed by the OS | Not started |
| 3 | `camera.hpp` | Camera devices and non-invasive capabilities exposed without activating capture | Not started |
| 3 | `printer.hpp` | Installed or discoverable printers, default printer, connection state, and basic capabilities | Not started |
| 3 | `bluetooth.hpp` | Local adapters, radio state, capabilities, and paired devices when permission permits | Not started |
| 3 | `wifi.hpp` | Wi-Fi adapters, radio state, current connection, signal information, and configured networks when permission permits | Not started |
| 3 | `software.hpp` | OS-managed services, drivers, updates, installed applications or packages, and runtime components where a stable system source exists | Not started |
| 3 | `process_list.hpp` | Other processes and their observable metadata, subject to permissions and platform privacy policy | Not started |
| 3 | `connection.hpp` | Local listening endpoints and network connections visible to the current process or user | Not started |

### Compile-time environment information

The planned `architecture.hpp`, `toolchain.hpp`, and
`execution_environment.hpp` headers form the Freestanding Minimal foundation.
They use allocation-free enums, integers, and compile-time constants, and they
do not include Hosted Full error or string facilities. Compile-target
information remains separate from runtime host information because a
cross-compiled binary's build target and runtime environment are different
concepts.

### Foundation implementation evidence

| Component | Target and environment | Toolchain | Evidence | State |
| --- | --- | --- | --- | --- |
| Freestanding Minimal public headers | `x86_64-pc-linux-gnu`, `-ffreestanding` | GCC 16.2.1 and Clang 22.1.8 | Strict C++17 compilation without Hosted Full headers | Compiles |
| Architecture, toolchain, and execution-environment detection | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 | GCC 16.2.1 with libstdc++ | Standalone headers, forced unknown target, forced generic backend, runtime assertions | Verified |
| Architecture, toolchain, and execution-environment detection | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 | Clang 22.1.8 with libstdc++ | Standalone headers, forced unknown target, forced generic backend, runtime assertions | Verified |
| Hosted error, result, capability, and UTF conversion foundation | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 | GCC 16.2.1 and Clang 22.1.8 | Strict C++17 unit, error mapping, invalid encoding, multi-translation-unit ODR, and sanitizer tests | Verified |

This evidence verifies only the listed compile target and host. Other enum
values and platform branches remain unverified until tested with their real
toolchains and targets.

### Operating-system module evidence

| Backend | Data sources | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results | Forced-backend standalone and runtime tests under GCC and Clang | Verified |
| Linux | `uname`, `gethostname`, `clock_gettime`, `/etc/os-release`, `/proc/stat`, and `/proc/sys/kernel/random/boot_id` | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44; strict C++17 GCC 16.2.1 and Clang 22.1.8; runtime, parser, ODR, error, and sanitizer tests | Verified |
| Windows | `RtlGetVersion`, `GetComputerNameExW`, and `GetTickCount64` | Backend implemented from public Windows APIs; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | `sysctlbyname`, `KERN_BOOTTIME`, and `gethostname` | Backend implemented from public Darwin APIs; no Apple SDK or runtime available in the current environment | In progress; uncompiled and unverified |

The Linux product fields follow the documented
[`os-release`](https://www.freedesktop.org/software/systemd/man/latest/os-release.html)
format. Boot time uses the kernel-documented
[`btime`](https://www.kernel.org/doc/html/latest/filesystems/proc.html) value
from `/proc/stat`, and the boot identifier uses the kernel-documented
[`boot_id`](https://www.kernel.org/doc/html/latest/admin-guide/sysctl/kernel.html).
The Windows implementation follows the public
[`RtlGetVersion`](https://learn.microsoft.com/en-us/windows/win32/devnotes/rtlgetversion)
and [system information](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/)
APIs. The macOS implementation follows the public
[`sysctl`](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/sysctlbyname.3.html)
interface. Windows and macOS statuses must not advance until their headers
compile with the official SDK and the tests execute on the real operating
system.

### Sensitive and identifying information

Machine identifiers, firmware serial numbers, storage serial numbers, hardware
UUIDs, network addresses, usernames, process command lines, paired devices, and
installed-software inventories can be sensitive. APIs that expose them must:

- require an explicit query rather than collecting them as a side effect;
- document privacy and permission implications;
- preserve permission errors;
- avoid logging, persistence, telemetry, and implicit network access; and
- avoid presenting unstable identifiers as permanent identity guarantees.

## Intended Capability by Platform Family

This table describes realistic intended coverage, not current implementation
status. **Broad** means that the platform generally exposes most of the domain.
**Partial** means that useful information exists but important fields are
restricted or absent. **Restricted** means the sandbox normally exposes only a
small subset. Every cell remains unimplemented until its backend is separately
marked Implemented or Verified.

| Domain | Linux | Windows | macOS | Android | BSD | Apple mobile | WASI/Web | Hosted fallback |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| OS and runtime | Broad | Broad | Broad | Broad | Broad | Partial | Restricted | Standard C++ only |
| CPU and topology | Broad | Broad | Broad | Partial | Broad | Partial | Restricted | Compile-time target only |
| Memory | Broad | Broad | Broad | Partial | Broad | Partial | Restricted | Not supported |
| Current process | Broad | Broad | Broad | Partial | Broad | Partial | Restricted | Standard C++ subset |
| Users and sessions | Broad | Broad | Broad | Restricted | Broad | Restricted | Not available | Not supported |
| Filesystems and mounts | Broad | Broad | Broad | Restricted | Broad | Restricted | Virtual filesystem only | Standard C++ subset |
| Storage devices | Broad | Broad | Broad | Restricted | Broad | Restricted | Not available | Not supported |
| Network configuration | Broad | Broad | Broad | Partial | Broad | Restricted | Restricted | Not supported |
| Power and batteries | Partial | Broad | Broad | Broad | Partial | Broad | Not available | Not supported |
| Hardware and firmware | Broad | Broad | Broad | Restricted | Partial | Restricted | Not available | Not supported |
| Displays and GPUs | Broad | Broad | Broad | Partial | Partial | Partial | Browser-provided subset | Not supported |
| Virtualization and containers | Broad | Broad | Partial | Partial | Broad | Restricted | Runtime-defined | Not supported |
| Security capabilities | Broad | Broad | Broad | Restricted | Partial | Restricted | Runtime-defined | Not supported |
| Sensors and device inventory | Partial | Partial | Partial | Broad with permission | Partial | Broad with permission | Browser permission model | Not supported |
| Software and process inventory | Broad | Broad | Partial | Restricted | Broad | Restricted | Not available | Not supported |

### Constrained and minimal capability targets

| Domain | Compatibility runtime | RTOS/Constrained | Freestanding Minimal | SDK Restricted |
| --- | --- | --- | --- | --- |
| Compile target and architecture | Broad, plus runtime identification | Broad when toolchain facts are available | Core guarantee | Unknown until SDK access |
| OS or runtime identity | Inherits host backend with explicit compatibility detection | RTOS identity and version when exposed | Explicit bare-metal or unknown classification | Unknown until SDK access |
| CPU, memory, and timing | Inherits host with possible restrictions | Partial through RTOS and board provider | Compile-target facts only | Unknown until SDK access |
| Process, user, filesystem, and network | Inherits the actual runtime | Conditional on configured RTOS facilities | Not available | Unknown until SDK access |
| Hardware, power, display, GPU, and sensors | Inherits host or product restrictions | Conditional on board provider and drivers | Explicit provider capabilities only | Unknown until SDK access |
| Security and software inventory | Inherits host permissions | Conditional and platform-specific | Not available | Unknown until SDK access |

The same evidence rules apply to every profile. A domain-level description does
not guarantee that every field is available on every release, runtime
configuration, machine, account, board, or SDK.

## Explicit Non-Goals and Boundaries

- No query performs an implicit internet request. Public IP addresses,
  geolocation, cloud instance metadata, remote inventory, and external service
  status are not local system information and are outside the default modules.
- Syscape does not bypass operating-system permissions, sandboxes, privacy
  controls, entitlements, or access-control policies.
- Syscape does not parse undocumented private kernel memory, use unsupported
  reverse-engineered interfaces, or require privileged drivers.
- Syscape does not guarantee that volatile values remain unchanged after a
  query returns.
- Syscape does not normalize genuinely different platform concepts into a
  misleading common value. Platform-specific additions remain possible under
  a clearly named namespace and without native types in the public interface.

## Implementation Milestones

1. **Dual-profile foundation:** implement allocation-free architecture,
   toolchain, and execution-environment detection independently from Hosted
   Full errors, `result<T>`, capability reporting, UTF-8 conversion, platform
   selection, fallback forcing, strict builds, and header/ODR tests.
2. **Tier 1 hosted core:** implement OS, CPU, memory, current process, user,
   filesystem, network, and locale information on Linux, Windows, and macOS.
3. **Tier 1 hosted extensions:** implement storage, power, hardware, display, GPU,
   virtualization, environment, and resource information on Tier 1 platforms.
4. **BSD and mobile:** port core modules to BSD, Android, Apple mobile, and
   HarmonyOS/OpenHarmony, respecting sandbox and permission limits.
5. **Other hosted and compatibility environments:** add public Unix-like,
   legacy, compatibility-runtime, and WebAssembly backends where strict C++17
   and acceptable platform sources exist.
6. **RTOS and providers:** add RTOS subsets through official APIs and explicit
   provider boundaries for board-specific information.
7. **Freestanding targets:** expand minimal architecture, toolchain, MCU, and
   board recognition without importing Hosted Full dependencies.
8. **Optional domains:** implement security, sensors, peripherals, software
   inventory, process inventory, and connection inventory with explicit
   privacy and permission behavior.
9. **SDK-restricted targets:** consider proprietary console backends only after
   lawful SDK access and permitted real-hardware verification are available.

## Updating This Matrix

Every change that adds or alters a query or backend must update this document.
The change must:

1. update the module status only after its portable API, fallback, tests, and
   documentation meet the repository completion rules;
2. name the exact platform family and relevant version or architecture in test
   evidence;
3. use **Verified** only after execution on the real target platform;
4. record runtime restrictions, permissions, and unavailable fields; and
5. update `platform-catalog.md` when a platform, architecture, runtime,
   toolchain, RTOS, MCU family, or classification changes; and
6. preserve the Hosted Full and Freestanding Minimal boundaries without
   weakening either profile's fallback contract.
