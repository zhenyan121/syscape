# Syscape Support Matrix and Information Catalog

Last reviewed: 2026-08-24

## Purpose

This document is the source of truth for the information Syscape intends to
expose and the support that has actually been implemented and verified. The
broader inventory of operating systems, compatibility environments,
architectures, toolchains, RTOS families, MCU families, WebAssembly hosts, and
SDK-restricted platforms is maintained in
[platform-catalog.md](platform-catalog.md).

Syscape uses layered language and compatibility profiles. Freestanding Minimal
requires strict C++11, while Hosted Full requires strict C++17. A platform does
not need to expose every item in this catalog, and a minimal target does not
need to compile headers declared for a larger profile. Every header must
compile on an unknown target that satisfies its declared language version and
profile. An unavailable Hosted Full query returns an error such as
`syscape::errc::not_supported`; an unrecognized Freestanding Minimal compile
target reports an explicit `unknown` enum value. Syscape never invents a value
to make a platform appear supported.

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

| Profile | Language and minimum contract | Error and text model |
| --- | --- | --- |
| Hosted Full | Strict C++17, the complete hosted standard library, and the full portable query API | `syscape::result<T>`, `std::error_code`, and UTF-8 `std::string` |
| Sandboxed/Restricted | Hosted or hosted-like strict C++17 with only publicly permitted information | Hosted error and text model; unavailable or denied data remains an explicit error |
| RTOS/Constrained | A per-header subset using official RTOS APIs and explicit board providers; every header declares its requirements, never below strict C++11 | Declared by each header according to available standard-library facilities |
| Freestanding Minimal | Strict C++11 allocation-free target, architecture, byte-order, toolchain, execution-environment, and board-capability facts | Enums, scalar values, compile-time constants, and caller-owned storage; no required `std::string` or `std::error_code` |
| SDK Restricted | Catalog entry only until lawful SDK access and permitted hardware verification exist | No API promise |

Profiles describe API availability, while implementation statuses describe
project progress. A target can be cataloged for Freestanding Minimal while all
of its Syscape modules remain Not started.

The CMake target `syscape::syscape` exposes the C++11 foundation without
raising a consumer to C++17. Applications using Hosted Full headers link
`syscape::hosted`, which links the foundation and propagates the C++17
requirement. A public header remains authoritative about its own minimum
language and library requirements regardless of the build-system integration.

## Platform Roadmap

Platform tiers define implementation order, not API quality. Once a backend is
claimed as supported, it must follow the same error, encoding, testing, and
documentation rules as every other backend.

### Foundation track: Hosted and freestanding foundations

Before platform backends, Syscape will design both the C++17 Hosted Full
foundation and the independent C++11 Freestanding Minimal compile-target
foundation. The minimal foundation must not include or depend on the hosted
error, string, or runtime-query layer.

### Tier 1: Primary desktop and server platforms

| Platform family | Initial variants | Intended platform sources | Current status |
| --- | --- | --- | --- |
| Linux | glibc and musl; x86, Arm, and RISC-V | POSIX APIs, Linux system calls, procfs, sysfs, netlink, and documented kernel interfaces | Implemented; all 29 hosted information modules and 6 freestanding foundation headers implemented; Linux x86-64 verified |
| Windows | Supported Windows client and server releases; x86, x64, and Arm64 | Win32, NT-supported public APIs, IP Helper, SetupAPI, registry APIs, and other Windows SDK facilities | Implemented; all 29 hosted modules implemented (unprivileged hardware sensors and SysV IPC return unsupported fallback); Windows 10.0.26100 x64 verified with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 |
| macOS | Supported macOS releases; x86-64 and Apple silicon | POSIX, `sysctl`, IOKit, SystemConfiguration, and other public system frameworks callable from C++ | Implemented; all 29 hosted modules implemented (unprivileged hardware sensors, CoreWLAN scanning, local socket inventory, and SysV IPC segments return unsupported fallback); strict C++17 Release build, 102/102 CTest tests, and examples verified on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK |

### Tier 2: BSD and mobile platforms

| Platform family | Initial variants | Intended platform sources | Current status |
| --- | --- | --- | --- |
| Android | Android NDK on Arm, Arm64, x86, and x86-64 | NDK APIs, Bionic system properties, procfs/sysfs available to applications, and POSIX APIs | Backend selection and standalone compile tests implemented across all 23 hosted modules. Active query backends implemented for core domains (OS, CPU, memory, process, process list, user, filesystem, network, environment, resource, power, storage, hardware, sensor, security, virtualization) using Android system properties (`__system_property_get`), safe procfs/sysfs access with SELinux EACCES handling, and POSIX facilities; restricted/unsupported domains (audio, connection, IPC, NUMA, printer, software) return portable `not_supported` fallbacks. API-level gates preserve API 21 compilation: interface enumeration requires API 24, text-encoding lookup requires API 26, and login-name lookup requires API 28; earlier targets return `not_supported` for the unavailable query. Note: network MTU queries via AF_INET socket require `android.permission.INTERNET`. Compile-tested with GCC and Clang; x86-64 emulator runtime testing is configured in CI, and physical on-device runtime verification remains pending. |
| FreeBSD | Supported releases and architectures available to the project | POSIX, `sysctl`, devstat, kenv, pciio, OSS audio, CUPS, and documented FreeBSD interfaces | Implemented across core OS, CPU, memory, process, user, filesystem, network, environment, resource, locale, IPC limits, virtualization, hardware, power, sensor, security, NUMA, process list, storage, software, audio, and printer modules; includes standalone tests for all 21 FreeBSD modules; source-reviewed awaiting FreeBSD hardware verification |
| OpenBSD | Supported releases and architectures available to the project | POSIX, `sysctl`, and documented OpenBSD interfaces | Implemented across core OS, CPU, memory, process, user, filesystem, network, environment, resource, locale, IPC limits, virtualization, hardware, power, sensor, security, NUMA, process list, storage, software, audio, connection, and printer modules; includes standalone tests for all 23 OpenBSD modules; source-reviewed against OpenBSD 7.8 sysroot interfaces, awaiting native OpenBSD hardware verification |
| NetBSD | Supported releases and architectures available to the project | POSIX, `sysctl`, and documented NetBSD interfaces | Implemented across core OS, CPU, memory, process, user, filesystem, network, environment, resource, locale, IPC limits, best-effort virtualization (hypervisor), hardware, security (ASLR), process list, storage (drives), software (services/packages), audio device inventory, and printcap printer inventory; power, sensor, NUMA, and connection provide strict unsupported fallbacks, and unlisted queries in partially implemented modules remain unsupported; includes standalone tests for all 23 NetBSD modules; source-reviewed against NetBSD 10.0 interfaces |
| DragonFly BSD | Supported releases and architectures available to the project | POSIX, `sysctl`, and documented DragonFly BSD interfaces | Implemented across core OS, CPU, memory, process, user, filesystem, network, environment, resource, locale, IPC limits, virtualization, hardware, power, sensor, security, NUMA, process list, storage, software, audio, printer, and connection modules; includes standalone tests for all 23 DragonFly BSD modules; source-reviewed against DragonFly BSD interfaces. Virtualized hosts can expose inconsistent boot-time clocks; `boot_time()` reports `malformed_data` instead of fabricating an instant in that case |
| Apple mobile platforms | iOS, iPadOS, tvOS, watchOS, visionOS, and Mac Catalyst | POSIX APIs, Mach task/host interfaces, sysctl, and Sandboxed/Restricted public Apple facilities | In progress (source backends implemented across 23 hosted query modules under the Sandboxed/Restricted profile using public Mach kernel APIs, BSD sysctl, dyld, and POSIX calls without private libproc interfaces; standalone test suite created; verification against Apple mobile SDK and physical device testing pending) |
| HarmonyOS and OpenHarmony | Native C++ environments on supported products (OpenHarmony Standard system / Linux kernel) under the Sandboxed/Restricted profile | OpenHarmony system parameter C APIs (dynamically loaded `libbegetutil.z.so`: `SystemReadParam` / `GetParameter`), procfs/sysfs with sandbox permission handling, and POSIX/Musl facilities | Implemented across all 23 hosted query modules under the Sandboxed/Restricted profile; active query backends implemented for core domains (OS, CPU, memory, process, process list, user, filesystem, network, environment, resource, power, storage, hardware, sensor, security, virtualization); unsupported peripheral domains return portable not_supported fallbacks; includes standalone tests for all 23 OpenHarmony modules; awaiting native OpenHarmony device verification. Note: System parameter interfaces (`libbegetutil`) belong to platform innerkits; restricted application sandboxes that disallow loading innerkit libraries or accessing restricted parameters report `not_supported` or `permission_denied` and fall back to POSIX/procfs where available. |

The visionOS CMake target requires CMake 3.28 or later. Other supported target
configurations retain the repository-wide CMake 3.15 minimum.

### Tier 3: Other hosted, compatibility, and sandboxed platforms

| Platform family | Intended scope | Current status |
| --- | --- | --- |
| illumos and Solaris | Hosted C++17 environments using POSIX and documented native interfaces | Implemented across core OS, CPU, memory, process, process list, user, filesystem, network, environment, resource, locale, virtualization (Solaris Zones), and hardware modules; audio, printer, NUMA, connection, software, storage, security, sensor, and power return unsupported fallbacks; includes standalone tests for all 23 Solaris modules; source-reviewed against illumos / Solaris interfaces |
| AIX | Hosted C++17 environments using POSIX and documented AIX interfaces | Not started |
| HP-UX | C++17-capable releases and toolchains using documented HP-UX interfaces | Not started |
| Haiku | Hosted C++17 applications using POSIX and public Haiku APIs | Implemented across core OS, CPU (logical count via get_cpu_info, topology core and package counts via get_cpu_topology_info, clock frequency via cpu_info), memory (physical, available, page size, load percent), process (pid, ppid, executable path via image_info, thread count, CPU times, memory usage, limits), process list (enumeration via get_next_team_info, real UID/GID, parent PID), user (POSIX identity and privilege), filesystem (capacity space, path limits, volume ID via dev_t and root ino_t), network (interfaces via getifaddrs, DNS resolver via resolv.conf), environment, resource (load average, process/thread counts, file descriptor limit), locale, and virtualization (CPUID hypervisor detection) modules; process command line, filesystem mounts, hardware, audio, printer, NUMA, connection, software, storage, security, sensor, and power return unsupported fallbacks; includes standalone tests for all 23 Haiku modules; registered in CI matrix via Haiku VM (r1beta5) |
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
| DOS, FreeDOS, OS/2, ArcaOS, and other legacy environments | The largest profile supported by a strict C++11-or-later toolchain and available runtime | Not started |

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
| 1 | `os.hpp` | Product name, version, build, kernel name and version, host name, boot time, uptime, and boot identifier where available | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 1 | `cpu.hpp` | Architecture, vendor, model, packages, physical and logical cores, topology, caches, instruction-set features, frequency, affinity, and utilization | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner. Cache geometry reports each platform's recorded values verbatim, while a backend that cannot identify distinct cache instances reports `not_supported`. Feature identifiers preserve each platform's own vocabulary instead of normalizing names across platforms. Affinity of the calling context belongs to the process module's scheduling slice rather than being duplicated here |
| 1 | `memory.hpp` | Physical memory, available memory, committed memory, swap or pagefile, page size, huge pages, pressure, and system utilization | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 1 | `process.hpp` | Current process identity, parent, executable, command line, working directory, start time, CPU time, memory use, priority, affinity, threads, and resource limits | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 1 | `user.hpp` | Current user identity, numeric or textual IDs, groups, home directory, shell, elevation, login session, active sessions, and logged-in users | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner. Fine-grained capability or per-privilege grants beyond the privilege classification remain not started |
| 1 | `filesystem.hpp` | Mounts, volumes, filesystem type, capacity, free space, block size, read-only state, and path limits | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 1 | `network.hpp` | Network interfaces, addresses, prefix lengths, MAC addresses, MTU, state, routes, default gateways, DNS configuration, and interface traffic and error statistics | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner. Interface enumeration with names, indices, operational state, loopback classification, link-layer addresses, MTU in bytes, unicast IPv4/IPv6 addresses with prefix lengths and numeric IPv6 scope identifiers, forwarding unicast routes, explicit default gateways, the system DNS resolver configuration with resolver addresses, ordered search domains where exposed, and separately recorded local domain name, as well as cumulative interface traffic and error statistics (rx/tx bytes, packets, errors, drops, multicast, and collisions) are complete. Host-name queries are provided by `os.hpp` rather than duplicated here |
| 1 | `locale.hpp` | Locale, preferred languages, country or region, text encoding, time zone, and UTC offset | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner. The current locale identifier, non-Unicode text-encoding label, and current UTC offset form the first slice; preferred languages, country or region, and time-zone identifiers form the second. Localized time-zone display names remain not started |
| 2 | `storage.hpp` | Physical drives, partitions, bus and media type, model, firmware, capacity, logical and physical sector sizes, rotational state, removable state, partition layout, and operating-system health / SMART diagnostics | Implemented; Linux whole-disk and partition enumeration verified; synthetic SMART parsers verified on Linux; real hardware NVMe/SCSI SMART ioctls unverified without administrative device node access; Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 2 | `power.hpp` | Batteries, charge, health, charging state, energy and charge capacities, instantaneous electrical measurements, power source inventory, and estimated remaining time | Implemented; battery enumeration with verbatim labels, presence, operating condition, health condition, technology, manufacturer, model, serial, whole-percentage charge estimates, health degradation percentage, energy capacities (design, full, current in mWh), charge capacities (design, full, current in mAh), voltage (terminal and min design in mV), power rate in mW, temperature in °C, completed-cycle counts, power source enumeration, external-power-source detection, and runtime-to-empty estimate implemented where documented platform sources exist. Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 2 | `hardware.hpp` | System manufacturer and model, chassis, motherboard, firmware or BIOS, hardware UUID, and documented device inventory | Implemented; system, motherboard, and firmware identity strings, SMBIOS chassis classification, firmware-recorded hardware UUID, and hardware device inventory (PCI/PCIe devices, USB bus devices, and SMBIOS physical memory modules/slots) are implemented where documented sources exist. Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 2 | `display.hpp` | Displays, bounds, resolution, work area, refresh rate, scale, orientation, color depth, and connection state | Implemented; Linux connector and EDID enumeration verified; Windows desktop/display queries verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 2 | `gpu.hpp` | GPU device enumeration, vendor classification, PCI IDs, model labels, driver names, VRAM capacity, and primary display adapter identification | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 2 | `virtualization.hpp` | Hypervisor presence and vendor, virtual machine hints, containers, namespaces, cgroups, jails, zones, WSL, and application sandboxing | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 2 | `environment.hpp` | Process environment snapshot, temporary and configuration directories, terminal presence, path resolution, and runtime environment characteristics | Implemented; environment-variable lookup, whole environment snapshot, standard user directories, current working directory, executable lookup in search path, path separators, and standard-stream terminal status implemented. Linux verified; Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 2 | `resource.hpp` | System load, scheduler information, system-wide process and thread counts, open-file and open-handle totals, handle or file descriptor limits, and other capacity limits | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 3 | `security.hpp` | Secure Boot state, TPM presence, privilege state, filesystem encryption visibility, integrity facilities, Address Space Layout Randomization (ASLR), CPU hardware vulnerability mitigations, process capabilities, and security capabilities publicly exposed by the OS | Implemented; Secure Boot, TPM, Linux Security Module, Linux lockdown, ASLR configuration mode, CPU hardware vulnerability table, POSIX process capabilities, token privileges, and volume encryption visibility implemented. Linux verified; Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 3 | `sensor.hpp` | Thermal zones, temperatures, fan speeds, and other sensors available through documented system interfaces | Implemented on Linux (verified); Windows and macOS stable unprivileged public OS APIs do not expose physical hardware temperatures or fan RPMs, and all queries honestly return `not_supported`; the Windows fallback is verified on the recorded host and the macOS fallback on the recorded GitHub-hosted runner |
| 3 | `audio.hpp` | Audio input and output devices, default devices, capabilities, and connection state | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 3 | `input.hpp` | Keyboards, pointing devices, touch, game controllers, and other input devices exposed by the OS | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 3 | `camera.hpp` | Camera devices and non-invasive capabilities exposed without activating capture | Implemented; Linux verified, Windows 10 version 1803-or-later verified on the recorded host; macOS backend verified on the recorded GitHub-hosted runner. Default-camera selection is currently unsupported because the implemented backends do not expose an authoritative platform default |
| 3 | `printer.hpp` | Installed or discoverable printers, default printer, connection state, and basic capabilities | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 3 | `bluetooth.hpp` | Local adapters, radio state, capabilities, and paired devices when permission permits | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 3 | `wifi.hpp` | Wi-Fi adapters, radio state, current connection, signal information, and configured networks when permission permits | Implemented; Linux verified, Windows verified on the recorded host; the macOS unsupported fallback is verified on the recorded GitHub-hosted runner |
| 3 | `software.hpp` | OS-managed services, drivers, updates, installed applications or packages, and runtime components where a stable system source exists | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 3 | `process_list.hpp` | Other processes and their observable metadata, subject to permissions and platform privacy policy | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 3 | `connection.hpp` | Local listening endpoints and network connections visible to the current process or user | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 3 | `numa.hpp` | Non-Uniform Memory Access (NUMA) node topology, CPU core-to-node mapping, per-node memory, inter-node distance matrix, and thread node location | Implemented; Linux verified, Windows verified on the recorded host; macOS verified on the recorded GitHub-hosted runner |
| 3 | `ipc.hpp` | POSIX and System V shared memory, message queues, semaphore sets, local UNIX domain sockets, and system IPC limits | Implemented on Linux (verified) and for macOS system IPC limits via `sysctl` (verified on the recorded GitHub-hosted runner); macOS local socket inventory and every Windows query return `not_supported`; the Windows fallback is verified on the recorded host |

### Compile-time environment information

The planned `architecture.hpp`, `toolchain.hpp`, and
`execution_environment.hpp` headers form the strict C++11 Freestanding Minimal
foundation. They use allocation-free enums, integers, and compile-time
constants, and they do not include Hosted Full error or string facilities.
Compile-target information remains separate from runtime host information
because a cross-compiled binary's build target and runtime environment are
different concepts.

### Foundation implementation evidence

| Component | Target and environment | Toolchain | Evidence | State |
| --- | --- | --- | --- | --- |
| Freestanding Minimal public headers | `x86_64-pc-linux-gnu`, including `-ffreestanding` | GCC 16.2.1 and Clang 22.1.8 | Strict C++11, C++14, and C++17 standalone, repeated-include, forced-fallback, and ODR checks without Hosted Full headers | Compiles |
| Architecture, toolchain, and execution-environment detection | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 | GCC 16.2.1 with libstdc++ | Standalone headers, forced unknown target, forced generic backend, runtime assertions | Verified |
| Architecture, toolchain, and execution-environment detection | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 | Clang 22.1.8 with libstdc++ | Standalone headers, forced unknown target, forced generic backend, runtime assertions | Verified |
| Hosted error, result, capability, and UTF conversion foundation | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 | GCC 16.2.1 and Clang 22.1.8 | Strict C++17 unit, error mapping, invalid encoding, multi-translation-unit ODR, and sanitizer tests | Verified |
| Freestanding Minimal and Hosted Full Windows suite | Windows 10.0.26100, x64 | MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Debug build with `/W4 /WX /permissive- /utf-8`; standalone public-header, C++11/C++14/C++17 minimal-profile, forced-fallback, multi-translation-unit ODR, Windows backend, CMake-standard, and runtime tests; all eight examples compiled and linked; CTest 103/103 passed | Verified on this host |

This evidence verifies only the listed compile target and host. Other enum
values and platform branches remain unverified until tested with their real
toolchains and targets. The Windows evidence covers the APIs and runtime paths
available to the unprivileged test process on the recorded host. It does not
claim real-device coverage for hardware that was absent, operations requiring
additional permissions, other Windows releases, x86 or Arm64, or clang-cl and
MinGW-w64.

### Operating-system module evidence

| Backend | Data sources | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results | Forced-backend standalone and runtime tests under GCC and Clang | Verified |
| Linux | `uname`, `gethostname`, `clock_gettime`, `/etc/os-release`, `/proc/stat`, and `/proc/sys/kernel/random/boot_id` | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44; strict C++17 GCC 16.2.1 and Clang 22.1.8; runtime, parser, ODR, error, and sanitizer tests | Verified |
| Windows | `RtlGetVersion`, `GetComputerNameExW`, and `GetTickCount64` | Backend implemented from public Windows APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | `sysctlbyname`, `KERN_BOOTTIME`, and `gethostname` | Backend implemented from public Darwin APIs; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

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
interface. The Windows status reflects compilation and execution on the recorded host.

### CPU identity and online-topology evidence

This first CPU slice exposes distinct platform-provided vendor identifiers and
model labels plus system-wide counts of online logical processors, physical
cores, and CPU packages. The counts intentionally do not apply process affinity
or container CPU quotas. Frequencies, utilization, cache topology, and
instruction-set features are covered by the slices below; per-caller
affinity is covered by the process scheduling-priority slice.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every CPU query | Forced-backend standalone, runtime, C++11-rejection, and ODR tests under GCC and Clang | Verified |
| Linux | `_SC_NPROCESSORS_ONLN`, architecture-specific `/proc/cpuinfo` identifiers and labels, `/sys/devices/system/cpu/online`, and per-CPU sysfs package/core IDs; identity queries may return `not_found` on architectures that expose no recognized field | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44; strict C++17 GCC 16.2.1 and Clang 22.1.8; parser, malformed-data, UTF-8, topology-invariant, standalone, runtime, ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests | Verified for this slice on the listed host |
| Windows | `GetActiveProcessorCount` and `GetLogicalProcessorInformationEx`; vendor identifiers and model labels return `not_supported`; requires Windows 7 or later processor-group declarations | Backend implemented from public Windows APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Documented `hw.logicalcpu` and `hw.physicalcpu` `sysctlbyname` values; package count, vendor identifiers, and model labels return `not_supported` | Backend implemented from public Apple APIs; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

Linux physical topology follows the kernel's documented
[CPU topology sysfs interface](https://docs.kernel.org/admin-guide/cputopology.html).
The Windows implementation follows the documented
[`GetActiveProcessorCount`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getactiveprocessorcount)
and
[`GetLogicalProcessorInformationEx`](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getlogicalprocessorinformationex)
APIs. The macOS counts follow Apple's documented
[system capability parameters](https://developer.apple.com/documentation/kernel/1387446-sysctlbyname/determining_system_capabilities).

### CPU frequency and utilization evidence

This second CPU slice exposes the platform-recorded minimum and maximum
operating clocks in kilohertz, one instantaneous current-clock reading per
online logical processor in kilohertz, and cumulative system-wide processor
time counters folded into user, system, and idle buckets. Recorded bounds
describe the platform's clock-selection range rather than measured behavior.
Current clocks are instantaneous samples that change continuously with
governor decisions. One usage tick is whatever unit the queried platform uses
for processor-time accounting; only differences between two snapshots carry
portable meaning, so callers compute utilization fractions from deltas.
Linux folds the documented aggregate `/proc/stat` fields as user plus nice,
system plus irq plus softirq, and idle plus iowait, while steal time is
deliberately excluded because it describes execution on behalf of other
virtual machines and belongs to no caller-visible bucket; guest time is not
added again because the kernel already counts it inside user and nice.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every frequency and utilization query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Kernel-documented [cpufreq sysfs attributes](https://docs.kernel.org/admin-guide/pm/cpufreq.html) supply recorded bounds (`cpuinfo_min_freq`, `cpuinfo_max_freq`) and current clocks (`scaling_cur_freq`); when the cpufreq interface is unavailable, which is common in virtual machines, current clocks fall back to the `/proc/cpuinfo` "cpu MHz" fields rounded half up to kilohertz, and the source switch is all-or-nothing so one vector never mixes sources; bounds require cpufreq and otherwise report `not_supported`; zero or nonnumeric frequency records are malformed platform data; utilization reads the kernel-documented `/proc/stat` aggregate line, whose absence reports `not_found`, with fewer than four counters or any nonnumeric counter treated as malformed data | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 (AMD Ryzen 9 8945HX). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic kilohertz, megahertz-rounding, cpuinfo-block, and aggregate-line parser tests including malformed, partial-coverage, orphan-field, negative, and overflow cases; live queries cross-checked for per-online-processor coverage, bound ordering, counter monotonicity between calls, and agreement of current clocks with an independent `/proc/cpuinfo` read; forced-fallback, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this slice on the listed host |
| Windows | [`CallNtPowerInformation(ProcessorInformation)`](https://learn.microsoft.com/en-us/windows/win32/api/powerbase/nf-powerbase-callntpowerinformation) records supply CurrentMhz and MaxMhz multiplied exactly by one thousand; requires linking PowrProf.lib, which the public header documents; the records expose no minimum operating frequency, so that query returns `not_supported`; its documented sizing procedure and [`GetSystemTimes`](https://learn.microsoft.com/en-us/windows/win32/api/proctimeapi/nf-proctimeapi-getsystemtimes) are group-relative on systems with more than one processor group, so frequency and utilization queries report `not_supported` there instead of silently partial coverage; `STATUS_ACCESS_DENIED` maps to `permission_denied` while other failing NTSTATUS values map to `io_error`; GetSystemTimes supplies cumulative idle, kernel, and user totals in hundred-nanosecond units, where system ticks are the kernel total minus idle because the kernel total includes idle, and a kernel total below idle is malformed platform data | Backend written from public Microsoft APIs with synthetic record-conversion, zero-frequency, null-buffer, NTSTATUS mapping, kernel-below-idle, group-count gate, and live monotonicity tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Documented `hw.cpufrequency_min` and `hw.cpufrequency_max` sysctl values converted from hertz with sub-kilohertz truncation; those keys are absent on Apple silicon, where both bound queries report `not_supported`; Darwin documents no public source for instantaneous clocks, so current frequencies return `not_supported`; utilization sums the documented [`host_processor_info`](https://developer.apple.com/documentation/kernel/1439494-host_processor_info) `PROCESSOR_CPU_LOAD_INFO` unsigned tick records, folding user plus nice into the user bucket, with the Mach port owned by an internal RAII guard and the implicit output buffer released through `vm_deallocate`; the underlying natural_t counters can wrap, so a caller must discard any interval whose later aggregate bucket is smaller; a failing kern_return_t maps to `io_error` | Backend written from public Apple APIs with synthetic per-processor tick-summing, high-bit-counter, and empty-input tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### CPU cache and instruction-set evidence

This third CPU slice exposes one entry per distinct processor cache instance
and the platform's instruction-set feature vocabulary. Two instances are
distinct when different sets of logical processors share them, so a
sixteen-core system lists sixteen level-one data caches; entries are ordered
by nondecreasing level and then by kind. Every geometry field reports the
platform's recorded value verbatim: associativity, set count, and sharing
count use the documented zero to record that the platform exposes no value,
because none of those quantities can be zero on real hardware, and a fully
associative cache converts to exactly one set holding size divided by line
size ways, which is the documented definition rather than a derived guess.
The feature query returns the platform's own vocabulary rendered verbatim,
so identifiers are comparable within one platform and deliberately not
normalized across platforms; an empty result is valid data meaning that the
platform answered without any present feature.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for both queries | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | The documented [testing sysfs cache ABI](https://docs.kernel.org/admin-guide/abi-testing-files.html) under `/sys/devices/system/cpu/cpuN/cache/indexI/` supplies `level`, `type`, `size`, `coherency_line_size`, `ways_of_associativity`, `number_of_sets`, and `shared_cpu_list`. This interface is documented but has not been promoted to Linux's stable ABI classification, so future kernels may evolve it; parsing remains strict and unsupported shapes fail honestly. The sharing set identifies an instance, so entries deduplicate across processors. The query intersects sharing sets with the observed online population, rereads that population after enumeration, and reports `temporarily_unavailable` for a changed population, contradictory duplicate geometry, or overlapping same-level same-kind instances instead of returning a torn snapshot. A processor without any cache directory contributes nothing and a population without any cache directory reports `not_supported`, which is common in virtual machines; the kernel unknown marker `-1` and attributes the platform omits both record as the not-reported zero, while other negative or nonnumeric renderings are malformed data; size renderings accept the documented kibibyte form plus defensive mebibyte and gibibyte suffixes, and zero sizes are malformed. Features collect the `/proc/cpuinfo` `flags` (x86 families), `Features` (arm64), and lowercase `features` fields, union every processor block in first-seen order, require all-or-nothing block coverage, and report `not_found` when the architecture exposes no recognized field, so compact encodings such as the RISC-V `isa` line are not yet represented | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 (AMD Ryzen 9 8945HX: 16 L1d, 16 L1i, 16 L2, and 2 L3 instances). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic size-suffix, type-mapping, attribute, unknown-marker, overflow, repeated-geometry, sharing-overlap, feature-token, duplicate, partial-coverage, orphan-field, and nonnumeric-index parser tests; live queries cross-checked for ordering, positive geometry, unique identifiers, and level-one data sharing counts that cover every online logical processor exactly once; forced-fallback, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this slice on the listed host |
| Windows | The documented [`GetLogicalProcessorInformationEx`](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getlogicalprocessorinformationex) `RelationCache` records supply level, associativity, line size, byte size, type, and the group mask whose popcount is the sharing count; `CACHE_FULLY_ASSOCIATIVE` converts to one set with size divided by line size ways; a zero level, line size, or size, an empty or foreign-group sharing mask, an unrecognized type, and a fully associative size that is not a line multiple are malformed data; single-group systems only, matching the other group-relative queries. Features call the documented [`IsProcessorFeaturePresent`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-isprocessorfeaturepresent) interface over its documented `PF_` instruction-set constants, including the currently published x86, Arm SVE, and Arm SME probes; each entry is guarded by its SDK constant so older SDKs omit newer facts. Operating-system and firmware facilities exposed by the same interface are outside the contract, and the rendered lowercase labels derive from the documented constant names | Backend written from public Microsoft APIs with synthetic record-conversion, fully-associative, trace, malformed-field, short-payload, affinity-popcount, and SDK-guarded feature-table tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Darwin's documented cache sysctls expose per-level geometry but no mapping from those values onto distinct sharing sets. Returning one aggregate entry per level would violate the portable per-instance contract and undercount multicore systems, so the cache query reports `not_supported`. Features collect the `machdep.cpu.features` and `machdep.cpu.leaf7_features` string sysctls where present and probe the documented `hw.optional` integer keys whose suffixes become the identifiers; values follow Apple's extensible nonzero-is-present convention, absent keys are skipped, and a platform that answers none reports `not_supported` | Backend written from public Apple sysctl interfaces with synthetic token-splitting, extensible optional-value, and cache-fallback tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Memory capacity and paging evidence

This first memory slice exposes the virtual-memory page size, total physical
memory, the operating system's estimate of allocatable-without-swapping
memory, and configured swap or pagefile usage. Committed-memory accounting,
huge pages, pressure, and system utilization remain not started. Linux reads
the kernel-documented [`proc_meminfo`](https://docs.kernel.org/admin-guide/proc-meminfo.html)
fields; Windows follows the documented
[`GlobalMemoryStatusEx`](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-globalmemorystatusex)
and [GetSystemInfo](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsysteminfo)
APIs and reports paging space as unsupported because those APIs expose only
process-scoped commit limits rather than paging-space capacity; macOS follows
the documented `hw.memsize` sysctl value, Mach
[host statistics](https://developer.apple.com/documentation/kernel/1439469-host_statistics64),
and the binary `xsw_usage` structure published by the `vm.swapusage` sysctl.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every memory query | Forced-backend standalone, runtime, C++11-rejection, and ODR tests under GCC and Clang | Verified |
| Linux | `_SC_PAGESIZE` plus `/proc/meminfo` `MemTotal`, `MemAvailable`, `SwapTotal`, and `SwapFree`; unknown fields are skipped before value parsing; available memory returns `not_supported` on kernels that predate `MemAvailable`; zero swap totals are valid data | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44; strict C++17 GCC 16.2.1 and Clang 22.1.8; parser, malformed-data, overflow, standalone, runtime, ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests | Verified for this slice on the listed host |
| Windows | `GlobalMemoryStatusEx` physical fields; `GetSystemInfo` page size; paging space returns `not_supported` because the pagefile fields describe commit limits scoped to the system or current process and cannot express paging-space capacity | Backend implemented from public Windows APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | `hw.memsize`; free and inactive pages from `host_statistics64` (volatile purgeable pages already reside in the kernel's inactive population); binary `xsw_usage` from `vm.swapusage`; Mach failures map to `io_error` because kern_return_t has no standard category; host send rights are owned by an internal RAII guard | Backend implemented from public Apple APIs; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Memory commit, huge-page, utilization, and pressure evidence

This second memory slice completes the module's planned coverage: virtual-memory
commit accounting, huge-page facts, physical-memory utilization, and
pressure-stall information. Commit accounting reports the currently committed
charge and the effective limit in bytes; there is deliberately no ordering rule
between them because platforms with heuristic overcommit legitimately report a
committed charge above the limit, and each backend documents its limit's scope.
The huge-page size query reports the platform's default application-visible
huge page verbatim, so gigantic sizes beyond that default (such as 1 GiB pages)
stay outside the contract; pool counts are page counts whose zero values are
valid empty-pool data. The utilization estimate is a whole percentage whose
definition is deliberately platform-specific rather than normalized: Windows
reports its documented load percentage verbatim, while Linux and macOS compute
the share of physical memory outside the same availability populations their
available-memory queries use. Pressure reports wall-clock stall fractions in
micro-percent (percent multiplied by exactly one million) so each platform's
two-fractional-digit rendering converts losslessly, plus cumulative stall time
in microseconds. Linux requires both records that its memory-pressure format
has documented since PSI was introduced instead of accepting a truncated
snapshot.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every new query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | `/proc/meminfo` `Committed_AS`, `CommitLimit`, `HugePages_Total`, `HugePages_Free`, and `Hugepagesize`; unitless count fields accept only bare integers while size fields require the documented kB suffix; utilization derives from MemTotal minus MemAvailable with exact overflow-checked integer rounding; pressure parses the kernel-documented [`PSI` records](https://docs.kernel.org/accounting/psi.html) requiring both the some and full records and all four assignments in each, exactly two fractional digits per average, averages within the documented 100-percent bound, an absent file mapped to `not_supported` for kernels built without pressure support, and unknown record kinds skipped defensively; free pool counts exceeding totals are malformed data | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 (AMD Ryzen 9 8945HX). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic kilobyte and bare-count parser tests, table-driven key classification including wrong-unit and suffix-less shapes, micro-percent boundary tests (zero, bound, one/three decimals, leading dot, beyond-bound, negative), pressure-record tests (both records, missing records, truncated assignments, malformed averages or totals, future kinds, duplicates, blank lines), live queries cross-checked against independent meminfo reads (exact commit-limit match, volatility-tolerant committed-charge match) and independent PSI reads plus cumulative-total monotonicity, huge-page power-of-two and page-size-ordering bounds, forced-fallback, standalone repeated-inclusion, C++11-rejection, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this slice on the listed host |
| Windows | [`GetPerformanceInfo`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getperformanceinfo) supplies the system-wide committed-page total, commit limit, and page size, which are converted to bytes with overflow checks; `GlobalMemoryStatusEx::dwMemoryLoad` reports the documented approximate percentage verbatim; [`GetLargePageMinimum`](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-getlargepageminimum) supplies the large-page minimum with a zero return mapped to `not_supported`; contradictory snapshot fields (available physical memory above total, a zero commit limit or performance page size, or a load above 100) are malformed data; pool counts and pressure return `not_supported` because the platform documents no acceptable source | Backend written from public Microsoft APIs with synthetic commit-page conversion, malformed-value, and overflow coverage; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Utilization computes used bytes as installed memory minus the free-plus-inactive populations that back available_memory_bytes(), rejecting contradictory statistics as malformed data and reusing the shared exact percentage helper; Darwin exposes no public commit-accounting, huge-page, or pressure-stall source, so those queries report `not_supported` | Backend written from public Apple APIs; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

Transparent Huge Pages configuration state is deliberately outside this slice:
it is administrative configuration rather than capacity or occupancy fact.
The Windows status reflects compilation and execution on the recorded host.

### Process identity and execution-context evidence

This first process slice exposes the current process ID, parent process ID,
executable path, command-line argument values, and current working directory.
Start time, CPU time, memory use, priority, affinity, thread count, and
resource limits are outside this slice; the runtime-attribute section below
covers start time, CPU time, memory use, and thread count, and the
scheduling-priority section below that covers priority, affinity, and
resource limits. An empty
argument list is valid data where
the operating system permits execution without arguments; ordinary executions
normally contain at least argv[0].

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every process query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | POSIX `getpid` and `getppid`; kernel-documented `/proc/self/exe` through `readlink`; NUL-framed `/proc/self/cmdline`; POSIX `getcwd`. The executable path is not canonicalized and may refer to a file that was later renamed or unlinked | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; command-line framing, empty arguments, missing terminator, invalid UTF-8, executable-path buffer growth and `EINTR`, standalone repeated inclusion, forced fallback, C++11 rejection, live-query, and two-translation-unit ODR tests passed | Verified for this slice on the listed host |
| Windows | `GetCurrentProcessId`; parent ID by scanning a `CreateToolhelp32Snapshot` process snapshot with `Process32FirstW`/`Process32NextW` (snapshot enumeration can be expensive); `GetModuleFileNameW(NULL)`; `GetCommandLineW` plus `CommandLineToArgvW` with `LocalFree`; two-stage `GetCurrentDirectoryW`; UTF-16 to UTF-8 conversion at the boundary | Backend written from public Microsoft APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | POSIX `getpid` and `getppid`; `_NSGetExecutablePath` reported verbatim and not canonicalized; `_NSGetArgc`/`_NSGetArgv`; POSIX `getcwd` | Backend written from public Darwin APIs; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Linux executable source follows the kernel's documented
[`proc`](https://docs.kernel.org/filesystems/proc.html) interface. The Windows
implementation follows public documentation for
[process identifiers](https://learn.microsoft.com/en-us/windows/win32/procthread/process-identifiers),
[tool-help snapshots](https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-createtoolhelp32snapshot),
[module files](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew),
[command lines](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw),
and [current directories](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getcurrentdirectoryw).
The macOS implementation follows Darwin's public dynamic-loader and program
argument interfaces plus POSIX `getcwd`. Windows and macOS
statuses must not advance until their headers compile with the official SDK
and their tests execute on those real operating systems.

### Process runtime-attribute evidence

This second process slice exposes the wall-clock start instant, user- and
system-mode execution times, resident and virtual memory extents, and the
live thread count of the calling process. Each query takes an independent
snapshot at the moment of the call. Execution-time resolution is limited by
each platform's accounting granularity: Linux reports clock ticks (commonly
ten milliseconds), Windows reports hundred-nanosecond units, and macOS
reports the task's nanosecond totals. Priority, affinity, and resource
limits are covered by the scheduling-priority section below. The resident
and virtual extents follow each platform's
own definition of those concepts and are documented as such rather than being
normalized into a single cross-platform meaning.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every runtime-attribute query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | One kernel-documented `/proc/self/stat` read supplies `utime`, `stime`, `num_threads`, `starttime`, `vsize`, and `rss`; parsing locates the parenthesized process-name boundary before field extraction; `sysconf(_SC_CLK_TCK)` scaling validates that the tick rate permits exact integer arithmetic and otherwise reports `not_supported`; RSS scales by `_SC_PAGESIZE`; the start instant derives from `/proc/stat` `btime` through the shared OS backend, so suspend periods and system-clock adjustments can shift it relative to real wall-clock time and it must be treated as an estimate; zero threads, negative or nonnumeric fields, and missing fields are malformed platform data; unrepresentable amounts report `value_too_large` | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic parser tests (parenthesized names, trailing newline, extra fields, truncated, nonnumeric, negative, zero-thread, oversized-thread, out-of-range tick lines), exact tick-rate arithmetic and overflow tests, page-scaling and start-composition boundary tests, live queries cross-checked for boot-to-now bounds, CPU-time monotonicity, resident-within-virtual ordering, and thread-count invariants, forced-fallback, C++11-rejection, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this slice on the listed host |
| Windows | `GetProcessTimes` creation, user, and system values converted from the documented 1601-epoch hundred-nanosecond FILETIME form with pre-epoch rejection; working set from the documented [`PROCESS_MEMORY_COUNTERS`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters) query declared in `<psapi.h>` (kernel32 mapping on Windows 7 or later SDKs or Psapi.lib linkage); address-space extent from a documented [`VirtualQuery`](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualquery) walk over reserved and committed regions behind an injectable seam covered by synthetic region chains; thread count from a documented [`TH32CS_SNAPTHREAD`](https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-createtoolhelp32snapshot) snapshot filtered by owning process, which can be comparatively expensive on systems with many threads | Backend written from public Microsoft APIs with FILETIME-conversion, epoch-boundary, region-walk, malformed-region, native-error, and overflow synthetic tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Documented `proc_pidinfo` `PROC_PIDTASKINFO` task fields supply nanosecond execution totals, resident and virtual sizes, and the thread count; the documented `KERN_PROC` sysctl supplies `p_starttime` for the recorded creation instant; microsecond components outside one second are malformed platform data | Backend written from public Apple APIs with duration-limit, time-pair, and malformed-component synthetic tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Process scheduling-priority, affinity, and resource-limit evidence

This third process slice exposes the platform-recorded scheduling priority,
the logical processor indices available to the caller's platform scheduling
context, and the recorded soft and hard bounds of six named resource-limit
kinds. Priority values use each platform's own documented scale and are not
comparable across platforms: Linux reports the calling thread's nice value,
macOS reports the calling process's nice value, and Windows maps the documented
priority-class constants onto their documented base priorities. Affinity
covers every Linux processor range for the calling thread. Windows reports
process affinity only on single-group systems, where group-relative bits are
also unambiguous system-wide indices; multiple-group systems report
`not_supported`. macOS exposes no documented public source and also reports
`not_supported`. Resource limits follow the POSIX `getrlimit` records for
core-file size, CPU time, file size, open files, stack size, and address
space; Windows exposes no per-process equivalent through a public documented
source and reports `not_supported` for every kind. An unlimited bound is
recorded as an explicit flag, never as a sentinel amount.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every priority, affinity, and resource-limit query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Linux `getpriority(PRIO_PROCESS)` reports the calling thread's nice value, validated against the kernel-documented range from -20 through 19; `sched_getaffinity` reports the calling thread's mask and expands a growing unsigned-long buffer with an `EINVAL` growth cap, yielding ascending unique indices, with an empty mask rejected as malformed platform data; `getrlimit` for `RLIMIT_CORE`, `RLIMIT_CPU`, `RLIMIT_FSIZE`, `RLIMIT_NOFILE`, `RLIMIT_STACK`, and `RLIMIT_AS` with `RLIM_INFINITY` recorded as an explicit unlimited bound and a soft-within-hard invariant | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic nice-range, mask-expansion, infinity-marker, signed-marker, soft/hard-ordering, and invalid-kind tests; live queries cross-checked against independent `getpriority`, `sched_getaffinity`, and per-kind `getrlimit` calls; forced-fallback, C++11-rejection, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this slice on the listed host |
| Windows | `GetPriorityClass` mapped onto the documented base-priority table from 4 (idle) through 24 (realtime) with unrecognized classes rejected as malformed data; `GetProcessAffinityMask` expanded only after `GetActiveProcessorGroupCount` establishes that group-relative bit positions are unambiguous system-wide indices, while multiple-group systems report `not_supported`; the process mask is validated as a subset of the system mask behind injectable validation and expansion seams; every resource-limit kind returns `not_supported` because the platform exposes no per-process equivalent; `_WIN32_WINNT` and `WINVER` values below 0x0601 are rejected, while absent values are scoped to 0x0601 only across the internal Windows SDK include boundary | Backend written from public Windows 7 or later APIs with synthetic class-mapping, group-count, and mask-expansion tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | POSIX `getpriority(PRIO_PROCESS)` validated against the Darwin-documented nice range from -20 through 20; no documented affinity source, so affinity returns `not_supported`; Darwin's signed 64-bit `rlim_t` uses the positive `RLIM_INFINITY` value 2^63−1 | Backend written from public POSIX interfaces with synthetic range, marker, and ordering tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### User identity and login sessions evidence

The user module exposes user identity, credentials, privileges, and active login sessions:
- `real_user_id()`, `effective_user_id()`, `real_group_id()`, `effective_group_id()` for numeric identifiers.
- `supplementary_groups()` for process group memberships.
- `privilege()` for elevated/privileged execution classification.
- `login_name()`, `user_name()`, `home_directory()`, `shell()` for user credentials.
- `sessions()` and `logged_in_users()` for system-wide active user login sessions.

Numeric identifiers are operating-system-scoped values; zero is valid where the
platform defines it and never acts as an error sentinel, and an empty
supplementary-group set is valid data meaning that the platform records no
membership beyond the effective group. Textual queries perform a fresh lookup
on every call, so concurrent user-database changes become visible between
calls.

The privilege classification reports whether the permission-checking identity
holds the platform's privileged account or an equivalent elevated token:
POSIX systems classify effective user identifier zero as privileged, and
Windows classifies the documented elevation state of the process token.
Finer-grained grants such as individual POSIX capabilities or Windows
per-privilege assignments are outside the classification and report
unprivileged rather than a fabricated elevated state.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every user query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | POSIX `getuid`, `geteuid`, `getgid`, and `getegid`; POSIX [`getpwuid_r`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/getpwuid_r.html) for the effective user supplies name, absolute home directory, and recorded shell; POSIX [`getgroups`](https://www.man7.org/linux/man-pages/man3/getgroups.3.html) supplies the supplementary set with insufficient-buffer retries capped before unbounded growth and reported ascending and unique because platforms document neither ordering nor uniqueness; privilege derives from `geteuid` alone; POSIX [`getlogin_r`](https://www.man7.org/linux/man-pages/man3/getlogin_r.3.html) supplies the controlling-terminal session name, whose absence conditions (`ENXIO`, `ENOTTY`, `ENOENT`) report `not_found` while other failures preserve their native codes; POSIX [`utmpx`](https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/utmpx.h.html) (`setutxent`, `getutxent`, `endutxent`) in-process parsing behind a process-wide mutex for `sessions()` and `logged_in_users()`, classifying terminal lines into console, graphical (`:0`), pseudo-terminal (`pts/*`), and remote network sessions, and converting `ut_tv` timestamps to `std::chrono::system_clock::time_point`. Zero external commands spawned; malformed records and invalid UTF-8 strings are rejected | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors` and high-signal warnings; buffer-growth (`ERANGE`) retry, permanently-insufficient-buffer, absent-entry, native-failure, malformed-entry, field-independence, group-normalization (unsorted, duplicated, empty), login-failure mapping, UTF-8 boundary, synthetic utmpx filtering/classification (empty, boot, dead, console, remote, graphical), invalid UTF-8 encoding rejection, live sessions and logged-in users enumeration, live-query identifier, group, and session cross-checks that tolerate hosts without a controlling terminal, forced fallback, C++11 rejection, standalone repeated inclusion, AddressSanitizer, UndefinedBehaviorSanitizer, and two-translation-unit ODR tests passed | Verified for this module on the listed host |
| Windows | `GetUserNameW` for the login name and `SHGetKnownFolderPath(FOLDERID_Profile)` for the profile directory, converted from UTF-16 at the boundary; [`OpenProcessToken`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-openprocesstoken) with [`GetTokenInformation(TokenElevation)`](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-gettokeninformation) classifies privilege, requires linking the Advapi32 import library, preserves native system errors including access-denied conditions, and reports a size mismatch as malformed data; Win32-facility HRESULT failures are narrowed to their Win32 code under the system category and other HRESULTs preserve their full value under an internal category, keeping permission failures comparable; all four numeric identifiers and the supplementary groups return `not_supported` because security identifiers are structured platform values rather than portable numbers; the login-session name returns `not_supported` because Windows exposes no acceptable source recorded separately from the process account; Windows Terminal Services APIs in `wtsapi32.h` ([`WTSEnumerateSessionsW`](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/nf-wtsapi32-wtsenumeratesessionsw) and [`WTSQuerySessionInformationW`](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/nf-wtsapi32-wtsquerysessioninformationw)) enumerate active sessions, connect states, client names/addresses, and logon FILETIME timestamps, linking `Wtsapi32.lib` | Backend written from public Microsoft APIs with synthetic invalid-token-handle failure-path tests, FILETIME conversion, and synthetic WTS session enumeration tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | The shared POSIX backend: `getuid`, `geteuid`, `getgid`, `getegid`, `getpwuid_r`, `getgroups`, `getlogin_r`, and `utmpx` (`setutxent`, `getutxent`, `endutxent`) | Backend written from public POSIX interfaces; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

User names, home directories, and login-session names can identify persons
or accounts. The queries are explicit, preserve permission errors, and
perform no logging, persistence, telemetry, or network access. The Windows
status reflects the recorded native host, while the macOS status reflects the
recorded GitHub-hosted runner rather than physical Apple hardware.

### Filesystem evidence

This first filesystem slice exposes the platform's mounted-filesystem table
and per-path volume capacity: total capacity, free capacity, the operating
system's unprivately-available estimate, allocation granularity, and
read-only state, all in bytes at the moment of the query. POSIX enumeration
reads only the mount table and never touches the listed volumes, so
unresponsive network filesystems cannot block it; Windows enumeration queries
each drive-letter volume and can block on unready media or unresponsive mapped
shares.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for valid filesystem queries; common public-boundary validation still rejects empty, embedded-null, and invalid-UTF-8 paths before backend selection | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Kernel-documented `/proc/self/mounts` with its four documented octal escapes decoded; POSIX `statvfs` for capacity with `f_frsize` block sizes; undocumented escape codes, truncated records, and a zero effective block size are malformed platform data; byte totals beyond 64 bits report `value_too_large`; missing paths preserve native errno | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic-record parsing (escapes, malformed lines, extra fields), statvfs scaling including `f_bsize` fallback, overflow, boundary validation, live enumeration cross-checked against an independent `getmntent` decode of the same table, live `statvfs` cross-checks, native-error, invalid-argument, invalid-encoding, forced-fallback, C++11-rejection, AddressSanitizer, UndefinedBehaviorSanitizer, repeated-inclusion, and two-translation-unit ODR tests passed | Verified for this slice on the listed host |
| Windows | `GetLogicalDrives` plus `QueryDosDeviceW` for sources, `GetVolumeInformationW` for file-system names and the read-only flag, `GetFileAttributesW` for preserving missing-path failures, `GetDiskFreeSpaceExW` for capacity, `GetVolumePathNameW` for resolving files, directories, junction points, and mounted folders to the volume actually holding the path, and `GetDiskFreeSpaceW` cluster size as the documented stand-in for a fundamental block size; only drive-letter volumes are enumerated, letters without ready media or locked volumes are omitted, device-mapping queries degrade to empty sources by design, and unlike POSIX enumeration the Windows enumeration contacts each drive-letter volume and may block on unready media or unresponsive network shares | Backend written from public Microsoft APIs with injectable enumeration seams covered by synthetic-data tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Documented `getfsstat` interface into caller-owned buffers with growth retries, POSIX `statvfs` for capacity | Backend written from public Apple APIs with synthetic `statfs` conversion tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Linux mount-table source follows the kernel's documented
[`proc`](https://docs.kernel.org/filesystems/proc.html) interface, whose
device, mount-point, and type fields encode whitespace as `\040`, `\011`,
`\012`, and backslash as `\134`. The Windows implementation follows public
documentation for
[drive letters](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getlogicaldrives),
[DOS devices](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-querydosdevicew),
[volume information](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumeinformationw),
[path attributes](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileattributesw),
[volume mount points](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumepathnamew),
[disk free space](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getdiskfreespaceexw),
and
[allocation granularity](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getdiskfreespacew).
The macOS implementation follows Darwin's documented
[`getfsstat`](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/getfsstat.2.html)
interface. The Windows status reflects compilation and execution on the recorded host.

### Filesystem path-limit and volume-identity evidence

This second filesystem slice completes the module's planned coverage:
per-volume path limits and an opaque volume identifier. The two path-limit
queries report each platform's recorded bound verbatim rather than being
normalized into one cross-platform unit: POSIX systems report the documented
`pathconf` `_PC_NAME_MAX` and `_PC_PATH_MAX` records in bytes (whether a
terminating null byte is included follows each platform's own limit
documentation), while Windows reports the documented `MaximumComponentLength`
record of `GetVolumeInformationW` in UTF-16 code units. A POSIX limit whose
value the platform documents as indeterminate is valid data recorded through
an explicit flag, never an error or sentinel; a determinate bound of zero is
malformed platform data because no component could be named. Windows reports
`not_supported` for the complete-path bound because the platform bounds
complete paths through process-wide activation policy rather than any
per-volume fact exposed by a documented interface. The identifier query
renders each platform's recorded opaque volume word pair verbatim as
fixed-width lowercase hexadecimal digits - Linux uses the kernel-documented
`statfs` `f_fsid` words, macOS uses the `statfs` `f_fsid` words, and Windows
uses the documented `GetVolumeInformationW` serial number - where an all-zero
rendering is valid data because platforms that define no distinguishing
identifier record zeros. Volume identifiers can distinguish machines or
volume instances; the query is explicit, performs no logging or persistence,
and documents that reformatting and remounting change the value, so it must
not be presented as a permanent identity guarantee.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for all three queries; common public-boundary validation still rejects empty, embedded-null, and invalid-UTF-8 paths before backend selection | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | POSIX `pathconf` with `_PC_NAME_MAX` and `_PC_PATH_MAX`, where errno is cleared first because POSIX distinguishes failure from indeterminacy only by the stored error; glibc answers `_PC_PATH_MAX` from a recorded constant without validating the whole path, so whether missing paths fail follows the implementation and is compared against independent reference calls rather than assumed; kernel-documented [`statfs`](https://www.man7.org/linux/man-pages/man2/statfs.2.html) `f_fsid` words copied bit by bit and rendered in recorded order, so the output never depends on integer endianness; interrupted calls retry | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic outcome-conversion tests (determinate, indeterminate, native failure, below-contract negative, zero-bound), fixed-width hexadecimal rendering vectors, validator tests (indeterminate normalization, zero-bound rejection, empty-rendering rejection), live component and complete-path bounds cross-checked against independent `pathconf` records including a missing-path comparison, identifier cross-checked against an independent `statfs` word-pair rendering plus stability and lowercase-hex invariants, pseudofilesystem zero identifiers accepted as valid data, forced-fallback, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this slice on the listed host |
| Windows | Documented [`GetVolumeInformationW`](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumeinformationw) supplies `MaximumComponentLength` and the serial word after shared resolution of files, directories, junction points, and mounted folders to the holding volume; the component length counts UTF-16 code units exactly as documented; the complete-path bound returns `not_supported` because MAX_PATH behavior is governed by process-wide activation policy rather than a per-volume documented fact; a determinate component bound of zero is malformed data | Backend written from public Microsoft APIs behind injectable volume-fact seams covered by synthetic-data tests including native-error preservation and zero-component rejection; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Shared POSIX `pathconf` backend for both limits and Darwin `statfs` `f_fsid` words rendered in recorded order | Backend written from public POSIX and Darwin interfaces with synthetic outcome-conversion, rendering-vector, and reference-comparison tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Network interface, route, and default-gateway evidence

The implemented network slices expose the platform's network interfaces with
their names, indices, operational state, loopback classification, link-layer
addresses, MTU in bytes, and unicast IPv4/IPv6 addresses with prefix lengths
and numeric IPv6 scope identifiers, together with forwarding unicast routes
and explicit default gateways, as observed during each query. The DNS
resolver configuration is covered by the dedicated evidence section below;
host-name queries belong to `os.hpp`. Interface rows of families outside
these slices are skipped without failing the query, and an interface without
unicast addresses is valid data. A scope identifier of zero is valid for IPv6
when the platform records no zone; IPv4 scope identifiers are always zero.
A zero MTU is valid when the platform records it for an interface that does
not transmit packets directly; it is preserved as data rather than used as an
unavailable-value sentinel. Route destinations are canonical network
addresses, on-link routes carry no next hop, and an empty default-gateway list
is valid. Metrics preserve the source platform's raw value and are not
comparable across operating systems.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every network query; shared validation preserves a platform-recorded zero MTU and rejects nonzero IPv4 scope identifiers, noncanonical route destinations, mismatched address families, and zero route interface indices | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Documented [`getifaddrs`](https://www.man7.org/linux/man-pages/man3/getifaddrs.3.html) enumeration with POSIX `if_nametoindex` indices and IPv6 `sin6_scope_id`; documented [`SIOCGIFMTU`](https://www.man7.org/linux/man-pages/man7/netdevice.7.html) returns each unique interface's MTU; documented [packet socket](https://www.man7.org/linux/man-pages/man7/packet.7.html) AF_PACKET rows supply link-layer addresses; a netmask with one bits after zero bits and an IPv4 row with a foreign netmask family are malformed platform data; a recorded hardware length beyond the eight-byte `sockaddr_ll` storage cannot be represented by this source and fails the snapshot with `not_supported` instead of being truncated; administratively up interfaces without running traffic report explicit unknown state. Kernel-documented [`NETLINK_ROUTE`](https://docs.kernel.org/userspace-api/netlink/intro.html) `RTM_GETROUTE` dumps supply IPv4 and IPv6 forwarding unicast routes; multipart response validation, interrupted dumps, kernel errors, raw priorities, and multipath next hops are handled explicitly, and link-local IPv6 gateways use the recorded output-interface index as their numeric scope. A compact `RTA_NH_ID` route without compatibility-expanded output-interface or multipath data reports `not_supported` until nexthop-object resolution is implemented; it is not misreported as malformed platform data | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; existing synthetic interface conversion and live cross-checks plus canonical-route boundary validation, synthetic default, multipath, nexthop-object and IPv6-scope route conversion, truncated-attribute rejection, live route and default-gateway invariants, forced-fallback, C++11-rejection, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this slice on the listed host |
| Windows | Documented [`GetAdaptersAddresses`](https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses) enumeration with `GAA_FLAG_INCLUDE_PREFIX`, documented buffer-growth retries capped before unbounded growth, friendly-name UTF-16 to UTF-8 conversion with ANSI identifier fallback, IPv4 `IfIndex` with `Ipv6IfIndex` fallback for IPv6-only adapters, `Mtu`, `OperStatus`, `IfType`, physical address, `OnLinkPrefixLength`, and IPv6 `sin6_scope_id` mapping, and injectable enumeration seams covered by synthetic-data tests; an adapter with both protocol indices zero cannot satisfy the portable nonzero-index contract and produces explicit `not_supported`. Documented [`GetIpForwardTable2`](https://learn.microsoft.com/en-us/windows/win32/api/netioapi/nf-netioapi-getipforwardtable2) supplies IPv4 and IPv6 routes; loopback, local-host, multicast, limited-broadcast, and interface-directed-broadcast rows are omitted with local address and prefix context from [`GetUnicastIpAddressTable`](https://learn.microsoft.com/en-us/windows/win32/api/netioapi/nf-netioapi-getunicastipaddresstable), unspecified next hops become on-link routes, `ERROR_NOT_FOUND` becomes a valid empty snapshot, and tables are released through `FreeMibTable`. Both sources require Windows Vista or later and linking Iphlpapi, which the public header documents | Backend written from public Microsoft APIs with synthetic MTU, zero-MTU, IPv6-scope, route conversion and filtering, empty-table, and release tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Shared POSIX `getifaddrs` backend reading IPv6 `sin6_scope_id` and Darwin's documented [`sockaddr_dl`](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/sockaddr_dl.3.html) AF_LINK rows, preserving link-layer addresses longer than six bytes through recorded length fields; `SIOCGIFMTU` supplies MTU by interface name. Darwin's public [`NET_RT_DUMP2`](https://github.com/apple/darwin-xnu/blob/main/bsd/sys/socket.h) `sysctl` source returns [`rt_msghdr2`](https://github.com/apple/darwin-xnu/blob/main/bsd/net/route.h) route records; variable-length socket addresses are bounds-checked with the source's four-byte alignment, down, reject, blackhole, unresolved, link-layer, local, broadcast, multicast, condemned, and dead entries are omitted, and `rmx_hopcount` is exposed as an optional raw metric | Backend written from public Darwin APIs with synthetic AF_LINK, MTU, IPv6-scope, IPv4/IPv6 route-message conversion, four-byte-alignment, filtering, metric, and truncation tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |
| Apple mobile | Public POSIX `getifaddrs`, `if_nametoindex`, and `SIOCGIFMTU` sources enumerate interfaces within sandbox permissions. The mobile public SDKs do not expose the `net/route.h` definitions used by the macOS `NET_RT_DUMP2` source, so routes and default gateways report `not_supported` rather than depending on unavailable SDK interfaces | Standalone Apple mobile tests cover the interface error contract and explicit unsupported route and gateway results; strict compilation in the Apple SDK CI matrix is pending a successful rerun | Implemented; SDK verification pending |

The Windows status reflects compilation and execution on the recorded host.

### Network DNS-configuration evidence

This final network slice completes the module's planned coverage: one
snapshot of the system DNS resolver configuration carrying the resolver
addresses in resolution-attempt order, the ordered global search domains
when the platform source exposes that list, and the local domain name where
the platform source records one separately. Empty present collections are
valid data meaning that the platform records no resolver or search list; an
absent search list means that the backend cannot expose that field. Duplicates
are preserved verbatim and the query performs no network requests. Each
server optionally records the interface the platform binds it to (Windows
per-adapter sources), and an IPv6
link-local resolver carries its numeric zone in the scope identifier.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for the whole snapshot | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | The documented [resolv.conf(5)](https://man7.org/linux/man-pages/man5/resolv.conf.5.html) file: `nameserver` supplies resolver addresses through the platform presentation-format converter, `search`/`domain` are mutually exclusive renderings of one list whose last instance determines both fields (a domain record is a single-entry list), comments are first-column semicolon or hash lines, and `options`, `sortlist`, and unrecognized directives are skipped exactly as the documented consumer ignores them. Beyond the documented literals this accepts the de-facto `address%zone` rendering that configuration tools emit for link-local resolvers and that glibc resolves to a numeric scope; the zone resolves through POSIX `if_nametoindex` at query time, and a zone naming a vanished interface skips only its own entry as an expected reconfiguration race while other unusable records fail as malformed platform data. Environment overrides such as `LOCALDOMAIN` and `RES_OPTIONS`, nsswitch policy, dynamically managed stub-resolver arrangements, and the consumer's own three-server cap (`MAXNS`) are outside this verbatim file view; an absent file reports `not_found` because the platform then records no DNS configuration | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 (NetworkManager-managed file with plain IPv4, scoped link-local IPv6, and a fourth server beyond the consumer cap). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic comment, tabulation, carriage-return, directive-override, malformed-record, scoped-literal, vanished-zone, empty-file, absent-file, native-error, and oversized-file tests plus boundary validation for UTF-8, emptiness, bindings, and trailing IPv4 bytes; live queries cross-checked against an independent read of `/etc/resolv.conf` and against glibc's parsed `_res` state agreeing on every numeric scope; forced-fallback, C++11-rejection, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this slice on the listed host |
| Windows | Documented [`GetAdaptersAddresses`](https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses) `FirstDnsServerAddress` chains concatenated in adapter enumeration order and preserved within each adapter including cross-adapter duplicates, each server bound to its adapter index with the documented IPv4 `IfIndex` preferred over `Ipv6IfIndex`, plus the documented [`GetNetworkParams`](https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getnetworkparams) global ANSI `DomainName` validated as UTF-8. These sources do not expose the distinct global suffix search list, so that field is unavailable rather than fabricated from the Vista-SP1-only per-adapter `FirstDnsSuffix` member. Unrepresented address families skip their entry, truncated socket addresses and unterminated fixed-size fields are malformed data, and the Windows resolver's own merge of these per-adapter records is outside this snapshot | Backend written from public Microsoft APIs with synthetic ordering, duplicate-preservation, binding-fallback, unknown-family, truncated-record, missing-address, unavailable-search-list, unterminated-field, encoding-failure, and native-error tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | The documented SystemConfiguration dynamic store entity `State:/Network/Global/DNS` with its schema keys for textual server addresses, search domains, and domain name; absent keys record absent fields, empty lists are valid data, wrong key or element types and unusable address renderings are malformed data, and an absent dictionary reports `not_found`; requires linking the SystemConfiguration and CoreFoundation frameworks, which the public header documents | Backend written from public Apple interfaces with synthetic full-extraction, partial-key, absent-dictionary, type-mismatch, malformed-address, scoped-address, empty-list, and ownership tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |
| Apple mobile | No documented public in-process source satisfying the portable resolver snapshot contract is used; the query reports `not_supported` | Standalone Apple mobile test asserts the unsupported result; strict compilation in the Apple SDK CI matrix is pending a successful rerun | Implemented; SDK verification pending |

The Windows status reflects compilation and execution on the recorded host.

### Network interface traffic and error statistics evidence

This network slice exposes instantaneous cumulative interface traffic counters, packet
counts, and transmission/reception error metrics: `rx_bytes`, `tx_bytes`, `rx_packets`,
`tx_packets`, `rx_errors`, `tx_errors`, `rx_dropped`, `tx_dropped`, `rx_multicast`,
and `collisions`. Queries are provided for all interfaces (`statistics()`), by interface
name (`statistics(name)`), and by interface index (`statistics(index)`).

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every statistics query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Kernel-documented `/proc/net/dev` interface and sysfs class `/sys/class/net/<iface>/statistics/` (`rx_bytes`, `tx_bytes`, `rx_packets`, `tx_packets`, `rx_errors`, `tx_errors`, `rx_dropped`, `rx_missed_errors`, `tx_dropped`, `multicast`, `collisions`). Name lookups use direct sysfs reads with `/proc/net/dev` fallback; sysfs `rx_dropped` and `rx_missed_errors` are added to match the folded `/proc/net/dev` drop semantics, with overflow detection. Index queries resolve through POSIX `if_indextoname`. Truncated lines, extra fields, malformed numbers, unknown lines, and non-UTF-8 names are rejected | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic `/proc/net/dev` table parsing, malformed-line and trailing-field rejection, overflow checks, empty name handling, UTF-8 boundary checks, live interface statistics, and lookup by name and index verified; forced-fallback, repeated-inclusion, two-translation-unit ODR, and C++11 rejection tests passed | Verified for this slice on the listed host |
| Windows | Official Win32 IP Helper APIs in `iphlpapi.h`: `GetIfTable2` and `GetIfEntry2` (`MIB_IF_TABLE2` / `MIB_IF_ROW2`), reading `InOctets`, `OutOctets`, checked sums of the unicast and non-unicast packet counters, `InErrors`, `OutErrors`, `InDiscards`, and `OutDiscards`. `rx_multicast` remains unavailable because this API exposes only a combined non-unicast packet count and separate multicast octets, not a multicast packet count. Statistics names are joined by interface index to the names exposed by `interfaces()` when the adapter table is available; otherwise they use the UTF-8-converted `Alias` with `Description` fallback. Applications must link `Iphlpapi.lib` | Backend written from public Microsoft Win32 APIs with synthetic `MIB_IF_TABLE2` conversion, overflow, null-table, and error-contract tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | `PF_ROUTE` `NET_RT_IFLIST2` sysctl records (`if_msghdr2`) reading 64-bit `if_data64` counters `ifi_ibytes`, `ifi_obytes`, `ifi_ipackets`, `ifi_opackets`, `ifi_ierrors`, `ifi_oerrors`, `ifi_iqdrops`, `ifi_imcasts`, and `ifi_collisions`, plus `ifm_snd_drops` for outgoing queue drops | Backend written from public Apple Darwin routing-socket definitions with synthetic 64-bit message conversion, malformed-message, and lookup-error tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |
| Apple mobile | The public mobile SDKs do not expose the routing message definitions used by the macOS `NET_RT_IFLIST2` source, so aggregate, name, and index statistics queries report `not_supported` | Standalone Apple mobile tests assert all three unsupported results; strict compilation in the Apple SDK CI matrix is pending a successful rerun | Implemented; SDK verification pending |

The Windows status reflects compilation and execution on the recorded host.

### Locale identity and UTC-offset evidence

This first locale slice exposes the process's current locale identifier, the
label of the encoding used for non-Unicode narrow text, and the local time
zone's UTC offset in effect at the moment of the query, in seconds east of
UTC. Locale identifiers and encoding labels are reported verbatim and are not
normalized across platforms: POSIX systems report the C runtime's locale
string and LC_CTYPE codeset name, while Windows reports the Microsoft C
runtime's locale string and decimal current multibyte code-page identifier.
Preferred languages, country or region, and time-zone identifiers are covered
by the second locale slice below. These queries observe C-runtime locale and
time-zone state, so concurrent changes to that state are a documented
unavoidable platform race. The Microsoft C runtime may additionally configure
locale state per thread.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every locale query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | The query form of POSIX [`setlocale`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/setlocale.html) for the locale identifier, POSIX [`nl_langinfo`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/nl_langinfo.html) `CODESET` for the encoding label, and `tzset` plus `localtime_r` with a C99 `strftime("%z")` rendering for the offset; an empty or undeterminable `%z` rendering reports `not_found`, and offset renderings outside the sign-plus-four-or-six-digit shape are malformed platform data | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic sign, digit-range, length, and extended-second offset-parsing tests, public-boundary empty, non-UTF-8, and offset-range tests, and live queries cross-checked against independent `setlocale`, `nl_langinfo`, and `sscanf`-decoded `strftime("%z")` references; forced-fallback, C++11-rejection, repeated-inclusion, and two-translation-unit ODR tests passed | Verified for this slice on the listed host |
| Windows | The query form of the Microsoft C runtime `setlocale` for the process locale, `_getmbcp` for the runtime's current multibyte code page rendered as a decimal label, and [`GetTimeZoneInformation`](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-gettimezoneinformation) with the bias component reported active for the instant; `TIME_ZONE_ID_UNKNOWN` still carries a valid base bias, dynamic time zones resolve through the same interface, and a summed bias reaching one day is malformed platform data | Backend written from documented Microsoft APIs with synthetic code-page rendering, bias-combination, and full-width `LONG` overflow tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Shared POSIX backend: `setlocale`, `nl_langinfo(CODESET)`, and `tzset` plus `localtime_r` with `strftime("%z")` | Backend written from public POSIX interfaces with synthetic parsing and boundary tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Linux locale and encoding sources follow the POSIX
[`setlocale`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/setlocale.html)
and
[`nl_langinfo`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/nl_langinfo.html)
specifications, and the offset follows the C standard `strftime` `%z`
conversion specifier. The Windows implementation follows public Microsoft
documentation for
[runtime locale and code-page selection](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/setlocale-wsetlocale)
and
[time-zone information](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/ns-timezoneapi-time_zone_information).
The Windows status reflects compilation and execution on the recorded host.

### Locale preference and time-zone identity evidence

This second locale slice completes the module's planned coverage: the ordered
list of language identifiers the user prefers, the country or region code
recorded by the platform's user configuration, and the identifier of the local
time zone. Every identifier is reported verbatim in its platform's own
vocabulary and is deliberately not normalized across platforms. The language
list preserves the platform's recorded preference order; entry order carries
no meaning beyond ranking within one platform, so entries are not comparable
across operating systems. The region code describes the configured locale's
region rather than a separately administered geographic setting on every
implemented platform. Time-zone identifiers name each platform's own zone
database entry: Linux extracts the identifier recorded by the documented
localtime configuration link, and Windows reports the dynamic time-zone
registry-key name, which is not an IANA identifier. macOS reports
`not_supported` because CoreFoundation substitutes GMT when the system zone
cannot be determined and exposes no way to distinguish that fallback from a
genuinely configured GMT zone. Localized display names remain outside this
slice. The queries observe user preference and time-zone configuration state,
so concurrent reconfiguration is a documented race.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every new query; shared boundary validation rejects empty or non-UTF-8 entries and an empty language list | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | The documented [localtime(5)](https://www.man7.org/linux/man-pages/man5/localtime.5.html) configuration link supplies the time-zone identifier when `TZ` is absent: the `/etc/localtime` symlink target is read with growth-bounded retries and resolved lexically against its containing directory, and only targets below `/usr/share/zoneinfo` yield an identifier, because inventing one from a basename would fabricate structure the platform does not record; a missing link records the documented `UTC` default, while a non-link configuration exposes no extractable identifier and reports `not_found`. The documented [tzset(3)](https://www.man7.org/linux/man-pages/man3/tzset.3.html) environment contract supplies process overrides: an empty `TZ` selects `UTC`, while a geographical file specification with or without its optional leading colon names a TZif file below the default database or `TZDIR`; roots and file paths are normalized lexically, targets must remain below the root, and a target must be a regular file carrying the TZif signature. A missing file or POSIX rule string records no extractable identifier and reports `not_found`; unusable file data reports `malformed_data`. Preferred languages return `not_supported` because message-translation environment overrides describe process state rather than platform configuration, and country or region returns `not_supported` because decomposing locale strings would normalize data the platform does not expose separately | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 (link target under `/usr/share/zoneinfo/Asia`). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic lexical-resolution tests (relative and absolute targets, dot segments, root-only targets, foreign roots, root-prefix traps, configured roots with trailing separators, and relative-base rejection) and language-boundary tests (ordering, empty list, empty and non-UTF-8 entries, failure propagation); live queries cross-checked against an independent `readlink` of `/etc/localtime`, empty-`TZ` UTC selection, geographical file forms with and without a colon, a trailing-separator `TZDIR`, non-file rejection, POSIX-rule rejection, and unsupported-source assertions; forced-fallback, C++11-rejection, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this slice on the listed host |
| Windows | [`GetUserPreferredUILanguages`](https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-getuserpreferreduilanguages) with `MUI_LANGUAGE_NAME` supplies the ordered null-delimited display-language list through two-stage sizing, with buffer padding after the terminator ignored and unconvertible UTF-16 failing the query; [`GetLocaleInfoEx`](https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-getlocaleinfoexw) `LOCALE_SISO3166CTRYNAME` reports the user locale's ISO 3166 region, with native second-call failures preserved and a successful size mismatch rejected as a torn snapshot; [`GetDynamicTimeZoneInformation`](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-getdynamictimezoneinformation) `TimeZoneKeyName` names the zone verbatim, where an unterminated or empty fixed-size field is malformed data; the APIs require `_WIN32_WINNT` and `WINVER` declarations of at least 0x0600, while absent macros are temporarily raised to the repository's current 0x0601 declaration floor so including `locale.hpp` first cannot hide Windows 7 declarations needed by another Syscape header | Backend written from public Microsoft APIs with synthetic list-splitting, padding, lone-surrogate conversion, key-name termination, empty-field, and unconvertible-field tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | [`CFLocaleCopyPreferredLanguages`](https://developer.apple.com/documentation/corefoundation/1542440-cflocalecopypreferredlanguages) copies the recorded language array verbatim in preference order with non-string elements rejected as malformed data and an empty recording rejected at the public boundary; `kCFLocaleCountryCode` from [`CFLocaleCopyCurrent`](https://developer.apple.com/documentation/corefoundation/1542013-cflocalecopycurrent) reports the locale's region, where an absent record reports `not_found`; references are owned by internal guards and string conversion failures preserve `invalid_encoding`. [`CFTimeZoneCopySystem`](https://developer.apple.com/documentation/corefoundation/cftimezonecopysystem%28%29) returns GMT when it cannot determine the system zone, so no observable result distinguishes failure from a genuinely configured GMT zone and the identifier query reports `not_supported` instead of fabricating success; requires linking the CoreFoundation framework, which the public header documents | Backend written from public Apple interfaces with synthetic language ordering, wrong-type, absent-region, string-boundary, ownership, and explicit unsupported-zone tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Environment variables, snapshot, paths, directories, and terminal status evidence

This environment module exposes environment variable lookup, existence checking,
whole environment variable snapshot (`environment_variables()`), current working directory
(`current_working_directory()`), executable search in `$PATH`/`%PATH%` (`find_executable()`),
platform path separators (`path_list_separator()`, `directory_separator()`),
standard filesystem directories (temporary directory, user home, user configuration,
user data, and user cache), and standard streams interactive terminal status
(stdin, stdout, stderr). All returned paths are guaranteed to be normalized
non-empty UTF-8 absolute paths without trailing directory separators (except
for the root directory).

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every environment query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Documented POSIX [`getenv`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/getenv.html) for variable retrieval; `environ` pointer for environment snapshots sorted by name; `getcwd` for working directory; `$PATH` scanning with `access(X_OK)` and directory rejection for executable lookup; XDG Base Directory specification (`$XDG_CONFIG_HOME`, `$XDG_DATA_HOME`, `$XDG_CACHE_HOME`) with `$HOME` and shared reentrant `getpwuid_r` fallback; `$TMPDIR` with `/tmp` fallback; POSIX [`isatty`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/isatty.html) for terminal checks. Queries do not mutate the process environment; callers must serialize C or POSIX environment mutation against queries because those APIs provide no portable reader/writer synchronization contract | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; variable existence, non-existence, empty values, invalid names, invalid UTF-8 values, environment snapshot sorting/comparison, working directory, executable resolution, path normalization, trailing slash trimming, XDG directories, temporary directory, and terminal status tests; forced-fallback, C++11-rejection, repeated-inclusion, and two-translation-unit ODR tests passed | Verified for this module on the listed host |
| Windows | Documented [`GetEnvironmentVariableW`](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getenvironmentvariablew) with UTF-16 to UTF-8 conversion; [`GetEnvironmentStringsW`](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getenvironmentstringsw) for environment snapshot with hidden-variable skipping; [`GetCurrentDirectoryW`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getcurrentdirectory) for working directory; [`SearchPathW`](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-searchpathw) with automatic extension lookup for executable resolution; [`GetTempPathW`](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettemppathw) for temporary directory; [`SHGetKnownFolderPath`](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath) for `FOLDERID_RoamingAppData`, `FOLDERID_LocalAppData`, and `FOLDERID_Profile`; `_isatty` combined with [`GetConsoleMode`](https://learn.microsoft.com/en-us/windows/console/getconsolemode) on `STD_INPUT_HANDLE`, `STD_OUTPUT_HANDLE`, and `STD_ERROR_HANDLE` | Backend written from public Microsoft APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | POSIX `getenv` with UTF-8 validation; `*_NSGetEnviron()` for environment snapshots; `getcwd` for working directory; `$PATH` lookup; `confstr(_CS_DARWIN_USER_TEMP_DIR)` with POSIX `/tmp` fallback; Standard macOS user directories (`~/Library/Application Support`, `~/Library/Caches`); POSIX `isatty` for terminal checks | Backend written from public Darwin and POSIX APIs; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### System resource and capacity evidence

This first resource slice exposes the system's one-, five-, and fifteen-minute
load averages, the scheduler's runnable and total entity counts, the number of
processes and threads existing system-wide, the operating system's open-file
total, its open-handle total, and the system-wide limit on open files or
handles. Every count is an instantaneous snapshot. Load averages are
dimensionless and are not normalized across platforms: Linux derives them from
runnable plus uninterruptible tasks, while macOS uses the BSD getloadavg
equivalent, and Windows exposes no documented load-average source at all. The
open-file total reports each platform's file-oriented notion, while the
separate open-handle total reports the platform's population of handles to all
kernel-object kinds; the two quantities are deliberately distinct queries, and
a platform that exposes only one of them reports `not_supported` for the other
rather than presenting unrelated data. Per-process resource limits remain in
the process module; CPU utilization is not part of this module.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every resource query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Kernel-documented `/proc/loadavg` supplies the three load samples, the running/total entity pair, and a validated final identifier field; process counts enumerate numeric `/proc/[pid]` entries; thread counts sum the `num_threads` field across `/proc/[pid]/stat` records, where only records whose process exits between listing and reading (a vanished entry reporting `ENOENT`, or an empty read) are skipped as expected races while permission, input, and format failures propagate, so a returned total always covers every readable record; cost grows with the process count and visibility follows procfs mount options such as `hidepid`; `/proc/sys/fs/file-nr` supplies allocated kernel file handles (first value) and the system-wide maximum (third value), where its sysctl rendering separates values with tabs; no documented source exists for an all-kernel-object handle total, so open_handle_count() returns `not_supported`; zero load samples, zero entities, and zero allocated handles are valid data | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic sample, loadavg-record, file-nr-record (including tab-separated rendering), stat-thread extraction (nested parentheses in names, truncated, zero-thread, nonnumeric), walk-propagation, validator, live cross-checks against independent `getloadavg` reads and per-process stat parsing, standalone repeated inclusion, forced-fallback, C++11-rejection, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this slice on the listed host |
| Windows | Documented [`GetPerformanceInfo`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getperformanceinfo) supplies ProcessCount, ThreadCount, and HandleCount from one call; requires Windows 7 or later SDK declarations with Kernel32.lib linkage or Psapi.lib; HandleCount counts every kernel-object handle kind rather than files only, so it backs the separate open_handle_count() query while open_file_count() returns `not_supported`; load averages return `not_supported` because processor-time counters measure interval utilization rather than damped demand, scheduling-entity counts return `not_supported`, and the descriptor-limit query returns `not_supported` because Windows documents no configurable system-wide handle limit; zero process or thread totals are malformed data because the calling process and thread must exist | Backend written from public Microsoft APIs with synthetic snapshot-validation tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Documented `getloadavg` interface for the three samples; process counts enumerate the documented `KERN_PROC_ALL` table through `sysctl` with growth retries capped before unbounded retries; the thread count reads `kern.num_threads`, the open-file count reads `kern.num_files`, and the limit reads `kern.maxfiles`; those three long-stable XNU sysctls are absent from Apple's formal documentation set and are used only because the platform exposes no stronger documented source; scheduling-entity counts and open_handle_count() return `not_supported` because Darwin documents neither a scheduling-entity nor an all-handles total | Backend written from public Darwin interfaces with synthetic nonnegative-scalar validation tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Linux sources follow the kernel's documented
[`proc`](https://docs.kernel.org/filesystems/proc.html) interface and the
[file-handle sysctl](https://docs.kernel.org/admin-guide/sysctl/fs.html)
documentation. The Windows implementation follows the public
[performance information](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getperformanceinfo)
API. The macOS enumeration follows Darwin's documented
[`sysctl`](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/sysctlbyname.3.html)
process-table interface. The Windows status reflects compilation and execution
on the recorded host.
### Power battery-state, metrics, and power supply evidence

The power module exposes the platform's recorded batteries and external power
supplies with verbatim UTF-8 labels, physical presence, operating condition,
health state, chemistry technology, manufacturer, model, serial number,
whole-percentage charge estimates, health degradation percentage relative to
design capacity, energy capacities (design, full, current in mWh), charge
capacities (design, full, current in mAh), instantaneous voltage in mV, power
rates in mW (positive charging, negative discharging), battery temperatures in
°C, completed-cycle counts, external power supply inventory (`power_sources()`),
active external source detection (`external_power_online()`), and the operating
system's runtime-to-empty estimate in seconds (`seconds_until_empty()`). Only
batteries satisfy the battery enumeration contract; external adapters, USB-PD
supplies, wireless chargers, and uninterruptible power supplies participate in
the power source and external-power queries.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every power query (`batteries()`, `power_sources()`, `external_power_online()`, `seconds_until_empty()`) | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | The kernel-documented testing [sysfs-class-power](https://docs.kernel.org/admin-guide/abi-testing-files.html) ABI under `/sys/class/power_supply` supplies `type`, `present`, `status`, `health`, `technology`, `manufacturer`, `model_name`, `serial_number`, `capacity`, `cycle_count`, `energy_full_design`, `energy_full`, `energy_now`, `charge_full_design`, `charge_full`, `charge_now`, `voltage_now`, `voltage_min_design`, `power_now`, `current_now`, `temp`, `online`, `voltage_max`; documented closed type set and status/health/technology vocabularies are parsed strictly, so undocumented renderings of recognized attributes fail as malformed data rather than being guessed; a missing attribute file records an absent field while any other native read failure propagates unchanged, and an entry whose attributes vanish between listing and reading is skipped as an expected removal race; zero cycle counts record their documented no-tracking meaning as an absent count; energy/charge/voltage/power attributes convert from micro-units to milli-units; temperature converts from deci-degrees to degrees Celsius; health degradation percentage computes ratio of full to design capacity; power supplies enumerate all non-battery supplies (`Mains`, `USB`, `USB_PD`, `Wireless`, `UPS`) sorted by identifier; external power follows active online states or a discharging battery; `seconds_until_empty()` returns `not_supported` as kernel documents no estimate source | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 (SMP internal battery plus AC and USB-C supplies). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic status, health, technology, power-source type, temperature, micro-unit numeric overflow, flag, online-state, cycle-count, and public-boundary UTF-8/range tests; live battery and power source enumeration cross-checked against sysfs attributes; forced-fallback, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this module on the listed host |
| Windows | Documented [`GetSystemPowerStatus`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-system_power_status) fields summarize every physical battery into one logical system battery; the unknown battery-flag rendering reports `not_found` because the population itself cannot be established, while the documented no-system-battery flag enumerates an empty list; the charging bit wins over AC-line evidence, an offline line means discharging, and online without charging means resting; the documented 255 percent marker records no estimate; the lifetime unknown marker reports `not_found` and zero seconds is valid exhausted-battery data; `power_sources()` returns `not_supported` because GetSystemPowerStatus provides only system line state rather than device inventory; the call lives in Kernel32 and needs no additional import library | Backend written from public Microsoft APIs with synthetic charging-bit precedence, offline, resting, unknown-marker, no-battery, presence-tri-state, power-source not_supported, and runtime-marker tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Documented [`IOPowerSources`](https://developer.apple.com/documentation/iokit/iopowersources_h) interfaces enumerate recorded power sources and external adapter details (`IOPSCopyExternalPowerAdapterDetails`) through caller-owned CoreFoundation guards; only `InternalBattery` types satisfy the battery contract while external adapters and UPS sources populate `power_sources()`, bound runtime estimates, and contribute external-power evidence together with `AC Power` states; the documented charging flag wins, followed by the fully-charged flag and then the source-state rendering; descriptions without names record empty identifiers; fractional charge estimates round half up exactly once and out-of-scale values are malformed data; cycle accounting stays absent; the smallest finite `kIOPSTimeToEmptyKey` value bounds the system runtime across batteries and UPS devices, its minus-one calculating marker reports `temporarily_unavailable`, and populations without estimates report `not_found`; requires linking the IOKit and CoreFoundation frameworks | Backend written from public Apple interfaces with synthetic battery-filtering, power-sources extraction, charging and fully-charged precedence, rounding, absent-cycle-accounting, presence-tri-state, duration-conversion, calculating-marker, malformed-negative, minimum-estimate, and UPS-bounding tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Storage drive, partition, and health diagnostics evidence

The storage module exposes hardware-backed whole-disk drives, partitions,
and operating-system drive health / SMART diagnostics recorded by the platform,
ordered by ascending identifier: `drives()`, `partitions()`, `disk_partitions(disk_identifier)`,
`health(disk_identifier)`, and `all_drive_health()`.

`drives()` exposes one entry per hardware-backed whole-disk drive. Each entry
carries the platform's verbatim identity strings (vendor, model, and firmware
revision), its recorded transport classification, its total capacity in bytes,
its logical and physical block sizes in bytes, whether the medium rotates, and
whether it is removable or ejectable. A zero capacity is valid data describing
a device holding no medium. Transport classifications preserve each platform's
own recorded vocabulary: Linux maps the kernel-recorded subsystem of the backing
device, where a SATA or USB disk behind the SCSI stack records scsi; Windows
renders its documented bus-type constants; macOS maps its recorded
DiskArbitration protocol descriptions.

`partitions()` exposes all storage partitions across all drives, and
`disk_partitions(disk_identifier)` queries partitions belonging to a specific
parent drive. Each entry carries the platform partition identifier, the parent
disk identifier, the 1-based partition number, starting byte offset, capacity
in bytes, partitioning scheme (MBR, GPT, Apple Partition Map, raw), type
identifier (MBR type hex or GPT type GUID), partition name/label, UUID,
filesystem type name, read-only flag, bootable/active flag, and current mount
point if mounted.

`health(disk_identifier)` and `all_drive_health()` expose physical drive
health status classification (`healthy`, `warning`, `critical`, `unknown`),
impending failure prediction flags (`failure_predicted`), operating temperature in
degrees Celsius (`temperature_celsius`), endurance percentage used (`percent_used`),
available spare percentage (`available_spare_percent`), cumulative power-on hours
(`power_on_hours`), power cycles (`power_cycles`), unsafe shutdowns (`unsafe_shutdowns`),
uncorrectable media errors (`media_errors`), and cumulative data units read and written
in bytes (`data_units_read_bytes`, `data_units_written_bytes`).

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every storage query (`drives()`, `partitions()`, `disk_partitions()`, `health()`, and `all_drive_health()`) | Forced-backend standalone and runtime tests under GCC and Clang | Verified |
| Linux | The kernel-documented sysfs block ABI under `/sys/block`: `size` in fixed 512-byte sectors with explicit overflow rejection, nonzero `queue/logical_block_size` and `queue/physical_block_size`, `queue/rotational`, `removable`, and `device/{vendor,model,rev}` with firmware padding trimmed. Partitions under `/sys/block/<disk>/<part>` parsed via `partition`, `start`, `size`, `ro`, and `uevent` (`PARTN`, `PARTNAME`, `PARTUUID`). GPT vs MBR scheme classified from standard 36-char hyphenated UUIDs vs 8-hex-digit disk-signature format; correlated with `/proc/self/mounts` for mount points and filesystem types. Drive health and SMART facts retrieved via NVMe Admin Get Log Page ioctl (`NVME_IOCTL_ADMIN_CMD`, Opcode 0x02, LID 0x02), SCSI/ATA pass-through ioctl (`SG_IO`), and sysfs hwmon temperature sensors (`/sys/block/<disk>/device/hwmon*/temp1_input`). Software constructs without backing hardware nodes are excluded | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic numeric overflow, flag, subsystem, UUID scheme classifier, partition boundary validation, synthetic NVMe 512-byte SMART log parser with 128-bit overflow rejection, synthetic ATA SMART return status parser, health boundary validation, and live query tests for drives, partitions, disk_partitions, and drive health cross-checked with sysfs; forced-fallback, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed. Real device SMART ioctl execution on live hardware remains unverified in this unprivileged environment | In progress; verified on the listed host for whole-disk, partition, and synthetic health slices |
| Windows | Present disks enumerated through SetupAPI disk device-interface class, `IOCTL_STORAGE_GET_DEVICE_NUMBER`, `IOCTL_STORAGE_QUERY_PROPERTY` for identity and block sizes, `IOCTL_DISK_GET_DRIVE_GEOMETRY_EX` for whole-disk capacity, `IOCTL_DISK_GET_DRIVE_LAYOUT_EX` (`DRIVE_LAYOUT_INFORMATION_EX`, `PARTITION_INFORMATION_EX`) for partition tables (MBR/GPT styles, partition numbers, offsets, lengths, boot indicators, MBR types, GPT type/id GUIDs, GPT partition names, and read-only attributes), and `IOCTL_STORAGE_PREDICT_FAILURE` for hardware SMART failure prediction. Volumes are enumerated independently through `FindFirstVolumeW`/`FindNextVolumeW`; `IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS` correlates them with partitions, `GetVolumePathNamesForVolumeNameW` supplies drive-letter and directory mount points, and `GetVolumeInformationW` supplies filesystem types. Variable-length extent buffers and UTF-16 text are validated before conversion. Unused container partitions are skipped | Backend written from public Microsoft APIs with synthetic descriptor, alignment, geometry, GUID formatting, UTF-16 name conversion, extent-buffer boundary validation, MBR/GPT layout conversion, volume correlation, partition enumeration, and failure prediction tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | IOKit `IOMedia` registry class and DiskArbitration framework (`DADiskCopyDescription` and `DADiskCopyWholeDisk`). Whole-disk media supply drive metadata and transport protocols. Partition candidates must explicitly record `kIOMediaWholeKey == FALSE`, resolve to a non-virtual whole disk accepted by the drive contract, and use that whole disk's BSD name as their parent. Partition media supply indices, base offsets, sizes, content-type renderings, read-only flags, volume names, CFUUID identifiers, and CFURL mount points. Content-type strings are not used to guess the parent partition-table scheme, so the scheme remains unknown when no documented table source is available. IOKit `IOBlockStorageDevice` `SMART Status` and `Statistics` dictionaries supply health condition and byte statistics | Backend written from public Apple interfaces with synthetic whole-media dictionary conversion, native CFUUID/CFURL conversion, partition dictionary conversion, parent-drive validation, partition collection, SMART status string parsing, and statistics dictionary tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Hardware identity and device inventory evidence

This hardware module exposes the firmware-recorded identity of the system, its
motherboard, and its firmware, the chassis form factor, the firmware-recorded
hardware UUID, as well as hardware device inventory: PCI/PCIe devices, USB bus
devices, and SMBIOS physical memory modules/slots.

Every text value is reported verbatim in UTF-8, so identity strings are
comparable within one platform and deliberately not normalized across
platforms. The chassis vocabulary is the SMBIOS System Enclosure Type table
itself rather than a Syscape invention: both implemented classification sources
publish that identical table verbatim. The hardware UUID is a machine identifier
with privacy implications; the query exists as a separate explicit call,
preserves permission failures, performs no logging or persistence, reports the
SMBIOS-documented all-zero and all-one renderings as `not_found` because they
distinguish nothing, and documents that firmware updates, board replacement,
and reimaging can change or clear the value.

Hardware device inventory queries provide:

- `pci_devices()`: Enumeration of observable PCI and PCIe devices with optional
  domain, bus, device, and function coordinates, vendor ID, device ID, optional
  subsystem vendor/device IDs, programming interface, class/subclass codes, portable `pci_class`
  classification, bound kernel driver or service name, and slot names, sorted
  deterministically by BDF.
- `usb_devices()`: Enumeration of connected USB bus devices with optional bus
  number, device address, upstream port, BCD release, class/subclass/protocol,
  manufacturer, product, serial number, and exactly represented speed in Mbps,
  plus required vendor and product IDs, sorted deterministically.
- `memory_devices()`: Enumeration of physical memory slots and installed modules
  (SMBIOS Type 17) with slot locator, optional bank locator, an explicit
  installed/not-installed/unknown state, optional capacity in bytes, form
  factor (`memory_form_factor`), memory technology (`memory_type`), rated and
  configured speed values carrying their SMBIOS-revision-defined MHz or MT/s
  unit (unknown where Linux exposes no table revision), manufacturer, serial
  number, and part number, sorted deterministically by locator.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every hardware query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | The kernel-documented [testing sysfs-class-dmi ABI](https://docs.kernel.org/admin-guide/abi-testing-files.html) under `/sys/class/dmi/id` for identity, `/sys/bus/pci/devices` for PCI enumeration, `/sys/bus/usb/devices` for USB enumeration, and raw SMBIOS structure table at `/sys/firmware/dmi/tables/DMI` for memory devices (SMBIOS Type 17). Required-attribute and directory-enumeration failures are preserved except for explicit device-removal races; optional malformed attributes fail rather than being dropped. USB 1.5 Mbps and nested `devpath` values remain exact, while the kernel's `unknown` speed rendering remains an absent optional value. Unprivileged access preserves native `permission_denied` errors on `product_uuid` and `/sys/firmware/dmi/tables/DMI` | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 (LENOVO 83LV). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic DMI-directory probes, decimal, hexadecimal, USB numeric/unknown speed and topology parsers, strict PCI BDF parser, PCI class classifier, memory form factor and type classifiers, endian-independent SMBIOS Type 17 parsing with empty slots, extended sizes, speed fields, required end markers, and trailing-data rejection; live PCI and USB device verification; forced-fallback, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this module on the listed host |
| Windows | Documented [`GetSystemFirmwareTable('RSMB')`](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemfirmwaretable) raw SMBIOS data parsing Type 0, 1, 2, 3, and 17 records; SetupAPI (`SetupDiGetClassDevsW` for `PCI` and `USB` enumerators) querying hardware IDs, compatible IDs, drivers, and bus numbers. PCI enumerator-only or virtual nodes without complete VEN_/DEV_ identities are skipped. USB composite-interface PDOs carrying `MI_` identifiers are omitted so each returned record represents a physical bus device, and descriptor-level class codes come from compatible IDs. SetupAPI's INF manufacturer, friendly-name, and device-description properties are not USB string descriptors, so Windows leaves the public descriptor-string fields unavailable. Enumeration/property failures and UTF-16 conversion failures are preserved, unavailable coordinates remain optional, and consumers link Setupapi.lib | Backend written from public Microsoft APIs with synthetic full-table, Type 17 memory record, empty-string-set rejection, hexadecimal, PCI hardware-ID, physical USB hardware-ID, composite-interface filtering, and USB compatible-ID parsers, singleton-duplicate rejection, and malformed-table tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | IOKit registry properties of `IOPlatformExpertDevice`, `IOPCIDevice`, and `IOUSBDevice` / `IOUSBHostDevice`; Open Firmware `assigned-addresses` values must be empty or contain complete 20-byte entries and use the first entry's `phys.hi` cell for BDF coordinates. `IOName` is a provider matching name rather than a physical slot identifier, so PCI slot names remain unavailable; memory device query returns `not_supported` | Backend written from public Apple interfaces with synthetic fact-interpretation, device-tree byte-string, scalar, empty, complete multi-cell, and truncated PCI property conversion, and tolerant live-query checks; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Virtualization and execution-context evidence

This module exposes hypervisor presence and vendor identification, container
runtime classification, Windows Subsystem for Linux (WSL) presence and version,
application sandbox classification, cgroup hierarchy version (v1, v2, hybrid),
process cgroup path, enabled controllers, active resource limits (memory.max,
cpu.max quota/period, pids.max), and Linux namespace enumeration and isolation
status (PID, Mount, Net, User, IPC, UTS, Cgroup, Time). On bare-metal execution
without hypervisors, containers, WSL, or sandboxes, `is_hypervisor_present()`,
`is_container()`, `is_wsl()`, and `is_sandboxed()` report `false`, their
corresponding enum queries report `none`, and text or version queries report
`not_found`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every virtualization query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | CPUID instruction leaf 1 ECX bit 31 and leaf 0x40000000 12-byte hypervisor signature string; DMI sysfs attributes under `/sys/class/dmi/id` (`sys_vendor`, `product_name`, `bios_vendor`); `/sys/hypervisor/type`; `/proc/device-tree/hypervisor`; `/run/systemd/container`; `/.dockerenv` and `/.containerenv`; `/proc/1/cgroup` and `/proc/self/cgroup`; `/proc/vz`; `/proc/sys/fs/binfmt_misc/WSLInterop`, `/run/WSL`, `/proc/version`, `/proc/sys/kernel/osrelease`, and `/dev/dxg` for WSL 1 vs WSL 2; `/.flatpak-info` and `SNAP` environment indicators; `/proc/self/cgroup`, `/sys/fs/cgroup/cgroup.controllers`, `/sys/fs/cgroup/*/memory.max`, `cpu.max`, `pids.max` for cgroups; `/proc/self/ns/*` symlink inodes and `/proc/1/ns/*` root namespace comparison for namespace isolation | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 (LENOVO 83LV). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic CPUID-signature decoder, sysfs type-token, DMI-matching, container-token, cgroup-line, namespace-classification, namespace-link parsing, cgroup-limit parsing, cgroup-cpu-max parsing, cgroup-controllers splitting, cgroup-proc-file parsing, and live-query tests; forced-fallback, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this module on the listed host |
| Windows | CPUID leaf 1 ECX bit 31 and leaf 0x40000000 signature via `__cpuid`; `OpenProcessToken` and `GetTokenInformation(TokenIsAppContainer)` for sandbox detection; native Windows host reports `is_wsl() = false`; cgroup and namespace queries report `not_supported` | Backend written from public Microsoft APIs with synthetic CPUID register parsing and bare-metal validation; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | `sysctlbyname("kern.hv_vmm_present")` and `machdep.cpu.features` VMM flag; `IOPlatformExpertDevice` model property matching for hypervisors; `APP_SANDBOX_CONTAINER_ID` environment indicator for App Sandbox; cgroup and namespace queries report `not_supported` | Backend written from public Darwin sysctl and IOKit interfaces with synthetic fact-interpretation tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Graphics processing unit (GPU) evidence

This module exposes the system's graphics processing units and display adapters.
`devices()` enumerates all detected GPU devices with their identifiers, model
labels, classified vendor names, PCI vendor/device IDs, kernel driver names,
dedicated VRAM capacity in bytes where exposed, and primary boot/display
classification. `device_count()` returns the total count, and `primary_device()`
returns the recorded primary boot or display adapter.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every GPU query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Documented sysfs PCI bus interfaces under `/sys/bus/pci/devices/` (matching display base class 0x03) and DRM class interfaces under `/sys/class/drm/`: `vendor`, `device`, `class`, `driver` symlinks, `boot_vga`, and driver-specific memory nodes (`mem_info_vram_total`). Devices are stably sorted by ID. Environments without PCI graphics or DRM adapters enumerate an empty list | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 (AMD Radeon 780M iGPU + NVIDIA RTX 4070 Laptop GPU). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic hex parser, PCI vendor classification, vendor name classifier, and live dual-GPU queries; forced-fallback, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this module on the listed host |
| Windows | Public Win32 `EnumDisplayDevicesW` interface parsing `DISPLAY_DEVICEW` properties (`DeviceName`, `DeviceString`, `DeviceID` PNP strings for `VEN_` and `DEV_` hexadecimal codes, and `DISPLAY_DEVICE_PRIMARY_DEVICE`) | Backend written from public Microsoft APIs with synthetic PNP ID parsing; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | IOKit registry matching `IOAccelerator` and `IOPCIDevice` services with `model`, `vendor-id`, and `device-id` properties | Backend written from public Apple IOKit interfaces with synthetic vendor classification; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Display and monitor evidence

This module enumerates display connectors and exposes only the display properties
that the selected backend can determine without heuristics. `displays()` returns
all detected outputs, including disconnected connectors where the platform lists
them, `display_count()` returns that list's size, and `primary_display()` returns
the main desktop display only when the platform identifies one.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every display query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Documented sysfs DRM connector interfaces under `/sys/class/drm/` (e.g. `card0-DP-1`, `card1-eDP-1`): `status`, `modes`, and binary `edid`. VESA EDID data up to the specification's 32 KiB maximum is accepted; the advertised extension count, exact length, and checksum of every 128-byte block are validated. The base block supplies manufacturer 3-letter PNP IDs, product codes, monitor name descriptors (tag `0xFC`), physical screen dimensions in millimeters, and preferred timing facts. Connector type and internal laptop panel classification (eDP, LVDS, DSI) derive from connector names. Stably sorted by ID. Connector sysfs lists supported modes but does not identify the compositor's current mode, desktop bounds, work area, scale, orientation, or primary display; those fields remain absent. A missing DRM class reports `not_supported`; an available class with no connectors returns an empty list | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 (eDP-1 internal panel). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic checksummed base and maximum-size extended EDID parsing, truncated and corrupt-extension rejection, detailed timing calculations, mode resolution parser, connector classifier, no-invented-desktop-state fixture, and live display queries; forced-fallback, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this module on the listed host |
| Windows | Public Win32 `EnumDisplayMonitors`, `GetMonitorInfoW` (`MONITORINFOEXW`), `EnumDisplaySettingsW` (`DEVMODEW`), and `EnumDisplayDevicesW` (`DISPLAY_DEVICEW`) for desktop coordinate bounds, work area, primary monitor flag, resolution, refresh rate, orientation, color depth, and monitor labels. Native failures and UTF-16 conversion errors are preserved; hardware-default refresh sentinels remain unknown | Backend written from public Microsoft APIs with synthetic UTF-8 and refresh-sentinel tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | CoreGraphics `CGGetActiveDisplayList`, `CGDisplayBounds`, `CGDisplayPixelsWide`/`CGDisplayPixelsHigh`, `CGDisplayCopyDisplayMode`, `CGDisplayModeGetRefreshRate`, `CGDisplayIsMain`, `CGDisplayIsBuiltin`, and `CGDisplayRotation`. Active displays are dynamically sized rather than capped, and consistent pixel-to-desktop dimensions provide the scale factor. Color depth remains absent because the public CoreGraphics display APIs used by this backend do not expose it | Backend written from public Apple CoreGraphics interfaces with synthetic UTF-8 and scale-factor validation; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Security, Secure Boot, and platform integrity evidence

This module exposes platform security, boot security, and integrity facilities:
`secure_boot()` and `is_secure_boot_enabled()` for UEFI Secure Boot state,
`tpm()` for Trusted Platform Module presence and version classification,
`security_modules()` for active kernel security modules (e.g. LSMs),
`lockdown()` for Linux kernel lockdown level, `is_sip_enabled()` for macOS
System Integrity Protection, `aslr()` for Address Space Layout Randomization mode,
`cpu_vulnerabilities()` for CPU hardware vulnerability mitigations,
`capabilities()` for fine-grained process capabilities,
`privileges()` for process token privileges,
`volume_encryption(path)` for filesystem volume encryption state, and
`encrypted_volumes()` for enumerating encrypted storage volumes.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every security query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Documented efivarfs interface under `/sys/firmware/efi/efivars/` (`SecureBoot-...`, `AuditMode-...`, and `SetupMode-...`), fallback legacy `/sys/firmware/efi/vars/` path, sysfs TPM class `/sys/class/tpm/` (`tpm_version_major` and backing device `uevent` DRIVER field), authoritative securityfs interfaces `/sys/kernel/security/lsm` and `/sys/kernel/security/lockdown`, `/proc/sys/kernel/randomize_va_space` for ASLR mode, sysfs `/sys/devices/system/cpu/vulnerabilities/` for hardware vulnerability mitigation status, `/proc/self/status` capability bitmasks (`CapInh`, `CapPrm`, `CapEff`, `CapBnd`, `CapAmb`) decoded into standard POSIX capability names (`cap_chown` through `cap_checkpoint_restore`), `/proc/self/mounts` and `/sys/block/*/dm/uuid` (`CRYPT-LUKS*`, `CRYPT-PLAIN*`) for LUKS and dm-crypt volume encryption visibility. Systems booted in legacy BIOS mode or without the Secure Boot variable report `not_supported`. Missing optional mode variables are tolerated after an explicit disabled SecureBoot value; malformed data, invalid UTF-8 public text, and all other native read failures are preserved | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44 (TPM 2.0 device). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; exact-size current and legacy efivar payload parsing, invalid-size and invalid-value rejection, fixture-backed enabled, disabled, audit, setup, missing-variable, and read-failure paths, known and future bracketed lockdown modes with malformed-data rejection, authoritative LSM parsing and missing-interface handling, invalid UTF-8 rejection, fixture-backed TPM version and backing-driver discovery, ASLR integer mode parsing, CPU hardware vulnerability mitigation classification, 64-bit capability bitmask decoding and status parsing, fixture-backed device-mapper LUKS/dm-crypt encryption parsing, volume path validation, and live security queries; AddressSanitizer and UndefinedBehaviorSanitizer passed with zero defects; forced-fallback, repeated-inclusion, two-translation-unit ODR, and C++11 rejection tests passed | Verified for this module on the listed host |
| Windows | Documented `GetFirmwareEnvironmentVariableW` for `SecureBoot` with GUID `{8be4df61-93ca-11d2-aa0d-00e098032b8c}`; this operation can require `SE_SYSTEM_ENVIRONMENT_NAME`. TPM Base Services `Tbsi_GetDeviceInfo` is resolved dynamically from the system `tbs.dll`, so including this header does not add a `Tbs.lib` link dependency. `GetProcessMitigationPolicy` with `ProcessASLRPolicy` for process ASLR mode. `OpenProcessToken` with `TokenPrivileges` and `LookupPrivilegeNameW` with dynamic buffer resizing for token privileges. Volume encryption and CPU vulnerability queries report `not_supported` until BitLocker/WMI integration is verified | Backend written from public Microsoft APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Standard TPM queries report an absent TPM because Apple platforms expose different security hardware semantics. Secure Boot, SIP, ASLR, CPU vulnerabilities, capabilities, privileges, and volume encryption currently report `not_supported` without public/permitted APIs verified against the Apple SDK | Fallback behavior implemented and covered by platform test source; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Hardware sensors, thermal zones, and fan speed evidence

This module exposes hardware monitoring sensors, temperatures in degrees Celsius,
fan speed probes in RPM, and operating-system thermal zones: `temperatures()`,
`fans()`, and `thermal_zones()`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every sensor query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Documented hwmon class interface under `/sys/class/hwmon/` (`temp*_input`, `temp*_label`, `temp*_max`, `temp*_crit`, `fan*_input`, `fan*_label`, `fan*_min`, `fan*_max`, `fan*_target`, and chip `name`), legacy nested `device/` attributes, and the thermal class interface `/sys/class/thermal/thermal_zone*` (`temp`, `type`, `mode`, and trip points `trip_point_*_type` / `trip_point_*_temp`). The class-device location takes precedence for a per-chip sensor index while the legacy location can supplement indices absent there, preventing duplicate probes without losing split-layout data. Millidegrees Celsius are converted to floating-point degrees Celsius; fan speeds are reported in RPM. An attribute that disappears between directory enumeration and reading is skipped as an expected device-removal race; malformed values, invalid UTF-8 public text, and every other native read failure are preserved | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic fixture-backed parsing for multi-sensor hwmon, nested and duplicate device layouts, negative temperatures, fan speed limits, thermal zones with passive/critical trip points, malformed required and optional attributes, native read failures, UTF-8 validation, and live hardware monitoring queries; AddressSanitizer and UndefinedBehaviorSanitizer passed with zero defects; forced-fallback, repeated-inclusion, two-translation-unit ODR, and C++11 rejection tests passed | Verified for this module on the listed host |
| Windows | Standard unprivileged Win32 APIs do not expose hardware temperature diodes, fan speed probes, or thermal zones without elevated WMI providers; every query honestly returns `not_supported` without fabricating placeholder values | Backend header present; runtime error contract tests verify that queries return `not_supported`; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Unsupported fallback; verified on the recorded Windows host |
| macOS | Stable public unprivileged APIs do not expose AppleSMC keys or per-diode hardware thermal sensors; qualitative thermal pressure states do not expose physical temperature measurements; every query honestly returns `not_supported` without fabricating values | Backend header present; runtime error contract tests verify that queries return `not_supported`; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Unsupported fallback verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Audio device, endpoint, and capability evidence

This module exposes system audio devices, input and output endpoints, stream
directions, direction-specific channel counts, sample rate capabilities, and
default endpoint detection where the platform exposes an authoritative source:
`devices()`, `playback_devices()`, `capture_devices()`, `default_playback_device()`,
`default_capture_device()`, and `device_count()`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every audio query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Documented ALSA procfs interfaces `/proc/asound/cards` and `/proc/asound/pcm`, with `/sys/class/sound/` used only to distinguish an empty ALSA installation from an unsupported subsystem. In-process zero-dependency parsing without linking `libasound` or PulseAudio. PCM devices are mapped to playback, capture, or duplex based on advertised stream capabilities. ALSA procfs does not authoritatively expose the desktop session's default endpoints or endpoint connection state, so default queries return `not_supported` and state remains `unknown`. Environments without audio hardware report an empty collection; environments without ALSA or kernel sound support report `not_supported` | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44 (HDA NVidia, HD-Audio Generic ALC287). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic cards, PCM streams, direction-specific default filtering, empty input, and malformed line parser tests; live queries verified for endpoint enumeration, stream direction invariants, UTF-8 validity, and explicit unsupported default lookup; forced-fallback, repeated-inclusion, two-translation-unit ODR, and C++11 rejection tests passed | Verified for this module on the listed host |
| Windows | Public Win32 Core Audio MMDevice interfaces (`IMMDeviceEnumerator`, `IMMDeviceCollection`, `IMMDevice`, `IMMEndpoint`, `IPropertyStore`) parsing `PKEY_Device_FriendlyName` and `PKEY_AudioEngine_DeviceFormat` for direction-specific channels and sample rates | Backend written from public Microsoft APIs with live endpoint, default-direction, and error-contract tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | CoreAudio HAL `AudioObjectGetPropertyData` on `kAudioObjectSystemObject` with `kAudioHardwarePropertyDevices`, `kAudioHardwarePropertyDefaultOutputDevice`, `kAudioHardwarePropertyDefaultInputDevice`, `kAudioDevicePropertyDeviceNameCFString`, `kAudioDevicePropertyDeviceUID`, `kAudioDevicePropertyStreamConfiguration`, `kAudioDevicePropertyDeviceIsAlive`, and `kAudioDevicePropertyNominalSampleRate` | Backend written from public Apple CoreAudio interfaces with live endpoint, direction-specific channel, default-direction, and error-contract tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Input devices module evidence

This module exposes connected and integrated input devices, device classifications
(keyboards, mice, touchpads, touchscreens, joysticks, gamepads, drawing tablets,
buttons/switches), hardware transport bus types, vendor/product/version identifiers,
physical paths, sysfs nodes, and handler bindings: `devices()`, `keyboards()`,
`mice()`, `touch_devices()`, `gamepads()`, and `device_count()`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every input query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Documented Linux input subsystem interfaces `/proc/bus/input/devices` and `/sys/class/input/`. In-process zero-dependency parsing without linking `libudev` or `libevdev`. Devices are classified into keyboards, mice, touchpads, touchscreens, gamepads, tablets, and buttons/switches based on device names, event handlers, and capability bitmasks (`EV`, `KEY`, `REL`, `ABS`, `PROP`, `SW`). Environments without input devices report an empty collection; environments without kernel input support report `not_supported` | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic device parsing (keyboards, mice, touchpads, touchscreens, gamepads, tablets, power/lid buttons, malformed lines), live queries verified for device enumeration, category filtering invariants, UTF-8 validity; forced-fallback, repeated-inclusion, two-translation-unit ODR, and C++11 rejection tests passed | Verified for this module on the listed host |
| Windows | Public Win32 Raw Input interfaces (`GetRawInputDeviceList`, `GetRawInputDeviceInfoW`) with `RIDI_DEVICEINFO` and `RIDI_DEVICENAME`, classifying `RIM_TYPEKEYBOARD`, `RIM_TYPEMOUSE`, and `RIM_TYPEHID` (Generic Desktop and Digitizer usage pages) | Backend written from public Microsoft APIs with device enumeration, category filtering, and error-contract tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Darwin IOKit `IOHIDManager` interfaces (`IOHIDManagerCreate`, `IOHIDManagerCopyDevices`, `IOHIDDeviceGetProperty`) querying product name, serial number, transport, vendor/product IDs, and primary usage/usage-page | Backend written from public Apple IOHID interfaces with device enumeration, category filtering, and error-contract tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Camera device, capability, and classification evidence

This module exposes camera and video devices, device nodes or interface paths,
non-invasive capabilities when the backend can establish them, optional transport,
integration, facing, and hardware identifier fields, and an explicit unsupported
result when no authoritative default-camera source exists. Enumeration does not
start video capture streams or deliberately illuminate camera indicator lights:
`devices()`, `capture_devices()`, `default_device()`, and `device_count()`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every camera query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Documented Linux sysfs interface `/sys/class/video4linux/` combined with non-invasive read-only non-blocking `VIDIOC_QUERYCAP` ioctls and parent USB topology parsing (`idVendor`, `idProduct`, `bcdDevice`, `removable`, `physical_location/panel`). `devices()` can return sysfs metadata when device-node access is unavailable and leaves capabilities unknown; `capture_devices()` requires a successful capability query and preserves device-node permission or I/O errors. Missing or wholly blank optional sysfs attributes record an absent value. Missing `/sys/class/video4linux` is indistinguishable from an unloaded subsystem with no devices and produces an empty collection. Entries use natural identifier ordering, so `video2` precedes `video10`. Video capture streaming is never activated. `default_device()` returns `not_supported` | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44 (SunplusIT Integrated Camera). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; live enumeration, natural identifier ordering, blank optional attributes, capability filtering, unknown-capability exclusion, panel parsing, hardware ID parsing, bounded V4L2 text handling, UTF-8 validation, forced generic fallback, repeated inclusion, two-translation-unit ODR, and C++11 rejection tests passed | Verified for this module on the listed host |
| Windows | Windows 10 version 1803 or later public Win32 SetupAPI enumeration restricted to `GUID_DEVINTERFACE_CAMERA`, with the runtime version checked through `RtlGetVersion`, device-instance-ID deduplication, and `SPDRP_FRIENDLYNAME`, `SPDRP_DEVICEDESC`, `SPDRP_HARDWAREID`, and `SPDRP_DRIVER`. Earlier Windows releases return `not_supported` rather than an ambiguous empty list. Interface membership establishes video capture; other capability, facing, integration, and default-camera fields remain unknown or unsupported instead of being inferred from names | Backend written from public Microsoft APIs with checked enumeration/property errors, bounded UTF-16 conversion, version-boundary logic, and hardware ID parsing tests; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | CoreMediaIO HAL enumeration (`CMIOObjectGetPropertyData` on `kCMIOObjectSystemObject` with `kCMIOHardwarePropertyDevices`, `kCMIODevicePropertyDeviceUID`, `kCMIODevicePropertyModelUID`, and `kCMIOObjectPropertyName`). Buffer sizes and returned Core Foundation types are validated. Stream direction/capability, connection, integration, facing, and default-camera classification are not inferred; `capture_devices()` and `default_device()` return `not_supported` | Backend written from public Apple CoreMediaIO interfaces with device-list size, property extraction, UTF-8 conversion, and error-contract tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Bluetooth adapter, radio state, and device query evidence

This module exposes local Bluetooth host adapters, radio power and rfkill block
states, hardware bus attachment classification, platform-available vendor and
version identifiers, and paired or connected remote devices: `adapters()`,
`adapter_count()`, `default_adapter()`, `paired_devices()`, and
`connected_devices()`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every Bluetooth query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Kernel sysfs `/sys/class/bluetooth/` and `/sys/class/rfkill/` combined with non-blocking AF_BLUETOOTH HCI socket `HCIGETDEVINFO` and `HCIGETCONNLIST` ioctls. Bus attachment is detected via parent subsystem symlinks. rfkill determines only the blocked condition; `HCI_UP`, `HCI_PSCAN`, and `HCI_ISCAN` determine on/off, connectable, and discoverable state without allowing an unblocked rfkill record to mask a powered-off controller. Mainline sysfs and `HCIGETDEVINFO` do not expose manufacturer or HCI/LMP version fields, so those optionals remain absent. An rfkill entry affects an adapter only when both its name and Bluetooth type match; directory, attribute, encoding, pairing-record, and connection-list socket or ioctl failures are preserved. Paired devices are parsed from `/var/lib/bluetooth/[adapter_mac]/` when permitted; connected devices are queried via kernel connection lists. Non-existent `/sys/class/bluetooth` produces an empty collection. `default_adapter()` designates `hci0` when present | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44 (MediaTek MT7921 / MT7922 Bluetooth). Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; live queries, natural identifier ordering including leading-zero tie-breaking, MAC normalization, synthetic rfkill/HCI power precedence, multi-adapter rfkill correlation, scan-flag decoding, invalid pairing-record encoding, unpaired-record filtering, class of device decoding, forced generic fallback, repeated inclusion, two-translation-unit ODR, and C++11 rejection tests passed | Verified for this module on the listed host |
| Windows | Win32 Bluetooth APIs in `BluetoothAPIs.h` (`BluetoothFindFirstRadio`, `BluetoothGetRadioInfo`, `BluetoothIsConnectable`, `BluetoothIsDiscoverable`, and `BluetoothFindFirstDevice`). `BLUETOOTH_RADIO_INFO` exposes the manufacturer and LMP subversion but not HCI or LMP major-version fields; unavailable fields remain absent rather than using driver-only interfaces. Non-invasive inquiry is enforced (`fIssueInquiry = FALSE`). Enumeration failures and malformed UTF-16 are preserved, bounded wide-string lengths use a standard C++ implementation, paired results are filtered to remembered or authenticated devices, and connected results also consider cached unknown devices | Backend implemented from public Microsoft Win32 APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Darwin IOKit and CoreFoundation IORegistry query matching `IOBluetoothHCIController` services. Address, manufacturer, numeric transport (USB, UART, or PCIe), legacy textual transport, and recognized numeric or Boolean power-state properties are decoded; an unavailable property remains `unknown`, and IOKit, property, and UTF-8 conversion failures are preserved. No matching service produces an empty collection. Paired and connected devices return `not_supported` to avoid invasive TCC permission prompts | Backend implemented from public Apple IOKit/CoreFoundation interfaces; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Wi-Fi adapter, radio state, and network query evidence

This module exposes local Wi-Fi host adapters, radio power and rfkill block
states, operational modes, active wireless connection details (SSID, BSSID, RSSI,
frequency, channel, Wi-Fi standard/generation, security protocol, link rates),
and configured network profiles: `adapters()`, `adapter_count()`,
`default_adapter()`, `current_connection()`, and `configured_networks()`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every Wi-Fi query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Kernel sysfs `/sys/class/net/` and adapter-correlated `/sys/class/net/[ifname]/phy80211/rfkill*` entries combined with `/proc/net/wireless` parsing and standard Wireless Extensions (WEXT) ioctls (`SIOCGIWNAME`, `SIOCGIWESSID`, `SIOCGIWAP`, `SIOCGIWFREQ`, `SIOCGIWMODE`, `SIOCGIWRATE`) over UDP sockets. Wireless interfaces are identified by the presence of `wireless` or `phy80211` directories under `/sys/class/net/[ifname]`. A nonzero, non-broadcast `SIOCGIWAP` BSSID establishes association; a retained configured ESSID alone never creates an active connection. WEXT `e == 0` channel-number results populate only the channel rather than being mislabeled as MHz, and other frequencies are accepted only inside a known Wi-Fi band. Configured profiles are parsed non-invasively from NetworkManager and the documented iwd `.open`, `.psk`, and `.8021x` stores when permission allows. An iwd `.psk` filename cannot distinguish WPA2-PSK from WPA3-SAE, so its security remains `unknown`; `.8021x` likewise identifies an enterprise profile without proving its WPA generation. Unexposed supported standards and bands remain empty. `default_adapter()` returns the sole adapter or sole connected adapter; ambiguous multi-adapter selection returns `not_supported` | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; live queries, adapter-correlated rfkill states, association-BSSID validation, WEXT channel-number and frequency conventions, natural identifier ordering, MAC normalization, frequency-to-channel and frequency-to-band conversion, RSSI to quality percentage estimation, synthetic `/proc/net/wireless` table parsing, NetworkManager and iwd profile parsing, forced generic fallback, repeated inclusion, two-translation-unit ODR, and C++11 rejection tests passed | Verified for this module on the listed host |
| Windows | Native Wifi API in `wlanapi.h` (`WlanOpenHandle`, `WlanEnumInterfaces`, `WlanQueryInterface`, `WlanGetNetworkBssList`, `WlanGetProfileList`, and `WlanGetProfile`). `WLAN_CONNECTION_ATTRIBUTES` exposes the active SSID, BSSID, signal quality, DOT11 PHY type, and auth algorithm. The channel-number opcode supplies a fallback channel, while the matching `WLAN_BSS_ENTRY` center frequency in kHz supplies an unambiguous frequency and 2.4/5/6 GHz band for Wi-Fi 6 versus 6E classification. BSS and radio-detail failures leave only those optional facts unknown; an interface disconnect race downgrades that adapter instead of failing the complete enumeration. TX/RX link rates remain absent because current Microsoft documentation does not define the units of `ulTxRate` and `ulRxRate`; real-hardware validation is required before mapping them to Mbps. XML profiles from `WlanGetProfile` are parsed for the nested or hexadecimal SSID, security type, auto-connect, and hidden SSID settings. Interface GUIDs and descriptions are decoded from UTF-16 to UTF-8. The call surface is available in Windows Vista-era `wlanapi.h`; documented numeric values are used for HE, EHT, and WPA3 algorithms so newer Windows 10 SDK enumerator spellings are not a compile-time dependency | Platform-independent XML entity, hexadecimal SSID, default-value, malformed-input, and WPA2/WPA3 classification paths compile and execute under the Linux GCC and Clang test configurations. The Native Wifi call layer is implemented from public Microsoft Win32 APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host; optional radio-dependent facts remain hardware-dependent |
| macOS | Portable fallback. No CoreWLAN implementation satisfying the header-only strict C++17 contract has been implemented and verified, so unavailable data is not inferred from unverified IORegistry properties | Forced generic behavior is covered by the portable fallback tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Unsupported until a documented system interface can be implemented and verified |

The Windows status reflects compilation and execution on the recorded host;
radio-dependent optional facts remain limited to hardware present during the run.

### Printer, default printer, queue status, and capability query evidence

This module exposes installed and discoverable printers, queue operational states,
connection type classification (local, network, virtual), job queue metrics, and basic
capabilities (color, duplex, paper sizes, resolutions, copy limits): `printers()`,
`printer_count()`, `default_printer()`, and `find_printer()`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every printer query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Native zero-dependency RFC 8010/8011 IPP queries over local CUPS Unix domain sockets (`/var/run/cups/cups.sock`, `/run/cups/cups.sock`, or `/var/run/cups.sock`) combined with `/etc/cups/printers.conf` configuration parsing and direct sysfs USB printer discovery (`/sys/class/usbmisc/lp[0-9]*`). Requests explicitly name every consumed IPP attribute. HTTP status, content length, chunked transfer framing, IPP framing, value bounds, and UTF-8 are validated; native access and I/O failures are preserved. CUPS queues and otherwise-unrepresented USB devices are merged using normalized queue, display-name, and percent-decoded USB URI identities without assigning unobserved queue state to raw devices. `default_printer()` honors nonempty `LPDEST`, then `PRINTER`, then the authoritative CUPS default, and never infers a default from queue count. Failure of the adjacent default query leaves a successful list available with unknown default flags, while `default_printer()` reports that failure. Entries sort in natural alphanumeric order | Strict C++17 GCC and Clang with `-pedantic-errors`, high-signal warnings, and `-Werror`; live queries tolerant of unavailable host facilities; synthetic content-length, chunked, HTTP error, IPP busy/service-unavailable, explicit requested-attribute, truncated IPP, invalid UTF-8, 1setOf, delimiter, configuration truncation, default-selection, realistic USB identity merge and unknown-state tests; forced generic fallback, repeated inclusion, two-translation-unit ODR, C++11 rejection, AddressSanitizer, and UndefinedBehaviorSanitizer | Verified for this module on the listed host |
| Windows | Official Win32 Spooler APIs in `winspool.h` (`EnumPrintersW` with `PRINTER_INFO_2W`, `GetDefaultPrinterW`, and `DeviceCapabilitiesW`). The enumeration sizing call is distinguished from a successful empty enumeration, buffer races are reported, and UTF-16 conversion failures fail the snapshot. Capability data calls allocate the documented library maximum rather than the potentially stale sizing count, then validate the returned count. Capabilities query paper names, color, duplex, collation, resolutions, and copy limits. The default comes only from `GetDefaultPrinterW`; the backend does not infer acceptance state because `PRINTER_INFO_2W` has no authoritative accepting-jobs field | Backend written from public Microsoft Win32 APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Native zero-dependency IPP queries over the macOS CUPS Unix domain socket (`/private/var/run/cupsd`) with compatibility paths and `/etc/cups/printers.conf` fallback. Requests explicitly name every consumed IPP attribute. Socket writes suppress SIGPIPE, descriptors are close-on-exec, HTTP/IPP framing and UTF-8 are validated, and errors are preserved. Failure of the adjacent default query leaves a successful list available with unknown default flags, while `default_printer()` reports that failure | Backend written from standard Unix domain socket and RFC IPP specification; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Process list and system process query evidence

This module exposes system-wide process enumeration, process count, per-PID metadata lookup,
and process lookup by executable or command name: `processes()`, `process_count()`, `find_process()`,
and `find_processes_by_name()`. Metadata that cannot be observed is represented by an empty
`std::optional`, preserving the distinction between an unavailable measurement and a valid zero
or empty value.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every process list query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Kernel procfs (`/proc/[pid]/stat`, `/proc/[pid]/status`, `/proc/[pid]/cmdline`, `/proc/[pid]/exe`, `/proc/[pid]/cwd`) combined with reentrant `getpwuid_r` username resolution and boot-time calculation. Processes are enumerated by scanning numeric `/proc` directories. Comm parsing robustly handles spaces and parentheses in process names. Unprivileged callers gracefully receive partial observable metadata for processes owned by other users or the kernel without failing the snapshot. Non-UTF-8 comm and argument bytes make only the affected optional fields unavailable; transiently truncated or malformed entries are skipped. `process_count()` counts the entries observable under the same rules as `processes()`, although independent calls can differ as processes start and exit. Non-existent PIDs report `not_found`. Entries sort in ascending order by PID | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; live queries, PID ordering invariants, self PID matching, non-existent PID not_found error, name lookup, a live child with non-UTF-8 comm and argv, synthetic stat parsing with nested parentheses/spaces, status UID/GID parsing, NUL-separated cmdline parsing, forced generic fallback, repeated inclusion, two-translation-unit ODR, and C++11 rejection tests passed | Verified for this module on the listed host |
| Windows | Win32 Toolhelp32 snapshot (`CreateToolhelp32Snapshot`, `Process32FirstW`, `Process32NextW`) combined with `OpenProcess`, `GetProcessTimes`, `GetProcessMemoryInfo`, `QueryFullProcessImageNameW`, and token-based `LookupAccountSidW` user name lookup. Unprivileged callers retain basic snapshot facts (PID, PPID, thread count, priority, exe name) when extended open access is denied. Toolhelp does not expose a reliable lifecycle state, so state remains `unknown`; `PROCESS_MEMORY_COUNTERS_EX` does not expose total virtual address-space size, so `virtual_memory_bytes` remains unavailable | Backend written from public Microsoft Win32 APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Darwin `sysctl` (`CTL_KERN, KERN_PROC, KERN_PROC_ALL`, `KERN_PROC_PID`) combined with `libproc` (`proc_pidinfo` with `PROC_PIDTASKINFO` and `proc_pidpath`) and `getpwuid_r`. Memory sizes, thread counts, CPU times, executable paths, and states are extracted with graceful degradation | Backend written from public Apple sysctl/libproc interfaces; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Network connection and socket inventory query evidence

This module exposes system-wide network connections, listening sockets, TCP states, endpoint
addresses and ports, owning PID correlation, and socket buffer queue occupancy: `connections()`,
`tcp_connections()`, `udp_endpoints()`, `listening_endpoints()`, and `find_connections_by_process()`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every connection query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Kernel procfs (`/proc/net/tcp`, `/proc/net/tcp6`, `/proc/net/udp`, `/proc/net/udp6`) combined with `/proc/[pid]/fd` scanning to map socket inodes to all observable owning PIDs. In-process zero-dependency parsing without invoking `ss` or `netstat`. Endpoints parse IPv4/IPv6 addresses in kernel byte order and 16-bit ports in host byte order. RFC 793 TCP states (1..11) map directly to portable `tcp_state`; UDP state remains `unknown`. Sockets with unspecified remote addresses and port 0 have `remote_endpoint` set to `std::nullopt`. Unprivileged callers retain connection records while unreadable process PIDs gracefully remain `std::nullopt`. Malformed records and table I/O failures are reported rather than silently omitted. Entries sort deterministically by protocol, complete local endpoint including scope, complete remote endpoint, state, PID, and inode | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; live connection enumeration, TCP/UDP filtering, listening endpoint validation, self PID lookup, synthetic IPv4/IPv6 hex parsing, malformed field rejection, TCP state decoding, send/receive queue decoding, shared inode to PID mapping, scope-aware ordering, forced generic fallback, repeated inclusion, two-translation-unit ODR, C++11 rejection, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this module on the listed host |
| Windows | Win32 IP Helper APIs in `iphlpapi.h` (`GetExtendedTcpTable` with `TCP_TABLE_OWNER_PID_ALL` for IPv4/IPv6 and `GetExtendedUdpTable` with `UDP_TABLE_OWNER_PID` for IPv4/IPv6). Applications must link the documented system library `Iphlpapi.lib`. `MIB_TCP_STATE_*` values map to `tcp_state`. Bounded buffer-sizing loops accommodate concurrent connection changes, validate returned table bounds, and preserve native errors. A table that explicitly reports `ERROR_NOT_SUPPORTED` is omitted while other table failures remain fatal; if every table is unsupported, the query returns `not_supported`. Remote endpoints evaluate to `std::nullopt` when unbound/listening | Backend written from public Microsoft Win32 APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Darwin `libproc` socket inspection (`proc_listpids` with `PROC_ALL_PIDS`, `proc_pidinfo` with `PROC_PIDLISTFDS`, and `proc_pidfdinfo` with `PROC_PIDFDSOCKETINFO`). Extracts `soi_kind`, `soi_protocol`, local/foreign socket addresses, IPv6 interface scope, TCP states, queue lengths, and associated process IDs. Duplicate descriptors for the same process/socket identity are collapsed while cross-process shared ownership remains represented | Backend corrected against current Apple XNU SDK declarations; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Software, service, driver, package, update, and runtime inventory evidence

This module exposes OS-managed system services and daemons, loaded kernel drivers and modules,
installed software packages and applications, observable system update state (with orthogonal `update_classification` and `update_severity`), and discovered runtimes with resolved semantic versions:
`services()`, `find_service()`, `loaded_drivers()`, `find_driver()`, `installed_packages()`,
`find_package()`, `system_updates()`, and `installed_runtimes()`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every software query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | In-process zero-dependency inspection of `/proc/modules` (driver name, memory footprint in bytes, reference counts, operational states), systemd unit directories (`/etc/systemd/system`, `/run/systemd/system`, `/usr/lib/systemd/system`, `/lib/systemd/system`) with `.wants`/`.requires` enable discovery and `/dev/null` mask detection, package manager catalogs (`/var/lib/pacman/local/` for Arch Linux, `/var/lib/dpkg/status` for Debian/Ubuntu, `/lib/apk/db/installed` for Alpine Linux), freedesktop `.desktop` files under `/usr/share/applications/`, `/usr/local/share/applications/`, and `~/.local/share/applications/`, update sources (`/usr/bin/checkupdates --nocolor` on Arch Linux, `/run/reboot-required.pkgs` and `/run/reboot-required` for packages pending system restart, plus PackageKit `/var/lib/PackageKit/prepared-update` key files and their legacy line-oriented package IDs), and installed runtimes with executable binary and version-directory validation across JVMs (`/usr/lib/jvm/`), .NET (`/usr/share/dotnet/`, `~/.dotnet/`), Python (`/usr/lib/python3.*`), Node.js (`~/.nvm/versions/node/`, headers), Rust (`~/.rustup/toolchains/` with `multirust-channel-manifest.toml` semantic version extraction), Go (the version-bearing first line of `/usr/lib/go/VERSION`), Ruby, and PHP. Arch update detection requires the optional `pacman-contrib` tool, synchronizes its isolated pacman database, treats exit status 2 as an empty successful result, and reports other command failures as temporarily unavailable. PackageKit package IDs require the documented four fields, malformed or empty update data is rejected, and public update and runtime text is validated as UTF-8. Missing update sources accurately return `not_supported`. Entries sort deterministically | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; live driver enumeration, live service enumeration, live pacman package query, live system updates, live runtime discovery, synthetic checkupdates/procfs/systemd/dpkg/apk/desktop/reboot/PackageKit/runtime parsers including valid updates, malformed checkupdates lines, valid comma-separated package IDs, empty optional fields, missing fields, extra fields, and empty prepared-update data, forced generic fallback, repeated inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this module on the listed host |
| Windows | Win32 Service Control Manager (`OpenSCManagerW`, `EnumServicesStatusExW` with `SERVICE_WIN32` for services with `lpResumeHandle` pagination, `QueryServiceConfigW` for startup types and binary paths), `EnumDeviceDrivers` via `psapi.h` for actively loaded kernel drivers, Windows Uninstall registry enumeration with dynamic key length querying via `RegQueryInfoKeyW` under `HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall`, `HKLM\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall`, and `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall`, Component Based Servicing updates (`HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\Packages` filtered for `0x60` Install Pending, plus `RebootPending` and `RebootRequired` indicators), and runtime discovery for .NET, Python, Java, and Node.js with NUL-validated dynamic registry strings, explicit 32-bit and 64-bit registry-view enumeration, and native error propagation. Applications must link `Advapi32.lib` and `Psapi.lib` | Backend written from public Microsoft Win32 APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | LaunchDaemons and LaunchAgents plist parsing (`/System/Library/LaunchDaemons`, `/Library/LaunchDaemons`, `/System/Library/LaunchAgents`, `/Library/LaunchAgents`), application bundle plist parsing via CoreFoundation (`/Applications/*.app/Contents/Info.plist`, `/System/Applications/*.app/Contents/Info.plist`, `/System/Applications/Utilities/*.app/Contents/Info.plist`, `~/Applications/*.app/Contents/Info.plist`), `/Library/Updates` plist parsing (`ProductPaths` and `Products` dictionaries with merged per-product metadata and `malformed_data` detection), and runtime discovery with executable and version-directory verification across Java Virtual Machines (`/Library/Java/JavaVirtualMachines` release files), Python Frameworks (`/Library/Frameworks/Python.framework/Versions`), .NET Core, Go, and Rust toolchains (`multirust-channel-manifest.toml` parsing). Public update and runtime text is validated as UTF-8. Loaded driver querying returns `not_supported` as macOS exposes no unprivileged public in-process API for loaded kernel extensions or DriverKit modules | Backend written from public Darwin file structures and CoreFoundation; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Non-Uniform Memory Access (NUMA) topology and node evidence

This module exposes NUMA node enumeration, CPU affinity per node, per-node physical
and free memory accounting, inter-node latency/distance matrix, and the calling thread's
current NUMA node: `is_numa_available()`, `node_count()`, `nodes()`, `node(id)`, and
`current_thread_node()`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every NUMA query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | In-process zero-dependency inspection of `/sys/devices/system/node/` (`online`, `possible`, `node[N]/cpulist`, `node[N]/meminfo`, `node[N]/distance`) and the `SYS_getcpu` system call or `sched_getcpu()`. Single-node UMA kernels report 1 node when the kernel exposes that node through the documented `online` attribute. A missing or inaccessible NUMA sysfs source produces an error instead of a fabricated UMA topology. Distances are populated when the kernel exposes the ACPI SLIT distance file; otherwise the distance vector remains empty. Missing files, permission errors, and I/O failures preserve their platform error codes. Malformed range strings or meminfo formats fail with `malformed_data`. Non-existent node IDs return `not_found`, and a directly queried possible but offline node reports `is_online == false`. Current thread NUMA lookup returns `temporarily_unavailable` if dynamic CPU mapping is unresolvable | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; live node enumeration, node count, CPU list range parsing, distance matrix parsing, node meminfo parsing, missing sysfs source propagation, possible-versus-online state, non-existent node lookup, thread node discovery, forced generic fallback, repeated inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this module on the listed host |
| Windows | Win32 NUMA APIs in `windows.h` (`GetNumaHighestNodeNumber`, `GetNumaNodeProcessorMask2` when available dynamically for multi-processor-group nodes, `GetNumaNodeProcessorMaskEx`, `GetNumaAvailableMemoryNodeEx`, `GetCurrentProcessorNumberEx`, and `GetNumaProcessorNodeEx`). Node enumeration iterates candidate IDs up to the highest node, checking node validity and skipping non-existent holes. Distance matrix remains empty because Windows Win32 APIs do not expose inter-node distance metrics | Backend written from public Microsoft Win32 APIs; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Verified on the recorded Windows host |
| macOS | Uniform Memory Access (UMA) architecture reporting a single unified node (Node 0) backed by `hw.logicalcpu`, `hw.memsize`, Mach `host_statistics64` free memory with RAII Mach host port deallocation (`host_port`). `is_numa_available()` reports `false`. Distance matrix remains empty | Backend written from public Apple Darwin sysctl and Mach APIs; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

### Inter-Process Communication (IPC) resource evidence

This module exposes POSIX and System V shared memory segments, message queues,
semaphore sets, active local UNIX domain sockets, and system-wide IPC resource limits:
`shared_memory_segments()`, `message_queues()`, `semaphore_sets()`, `local_sockets()`,
and `limits()`.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every IPC query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | In-process zero-dependency inspection of `/proc/sysvipc/shm`, `/proc/sysvipc/msg`, `/proc/sysvipc/sem`, `/proc/net/unix`, `/proc/[pid]/fd` scanning for socket inode-to-PID correlation, `/dev/shm` for candidate POSIX shared memory (regular files, noting that `/dev/shm` is a tmpfs mount where any application regular file is enumerated) and named semaphores (`sem.*`), `/dev/mqueue` for POSIX message queues, and `/proc/sys/kernel/` (`shmmax`, `shmall`, `shmmni`, `msgmax`, `msgmnb`, `msgmni`, `sem`) for system IPC limits. Non-numeric or truncated fields fail with `malformed_data`. Non-UTF-8 paths or names fail with `invalid_encoding`. If all underlying subsystems are absent/unsupported, queries return `not_supported`; permission and I/O errors are strictly preserved. Sockets accurately map stream/datagram/seqpacket types, unconnected/connecting/connected/disconnecting/listening states, and abstract socket names (`@`). Entries sort deterministically | Arch Linux, Linux 7.1.9, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; live queries, synthetic SysV SHM/MSG/SEM parsing with signed keys, synthetic `/proc/net/unix` parsing, socket flag decoding, inode PID mapping, UTF-8 validation, deterministic sorting invariants, forced generic fallback, repeated inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this module on the listed host |
| Windows | Windows Win32 does not provide System V or POSIX IPC subsystems, and does not provide standard UNIX domain socket table enumeration; every IPC query honestly returns `not_supported` without fabricating unrelated system metrics | Backend header present; runtime error contract tests verify that queries return `not_supported`; compiled and exercised on Windows 10.0.26100 x64 with MSVC 19.51.36248 and Windows SDK 10.0.26100.0 | Unsupported fallback; verified on the recorded Windows host |
| macOS | System V IPC limits are queried via `sysctlbyname` with dynamic size discovery over `kern.sysv.shmmax`, `kern.sysv.shmall`, `kern.sysv.shmmni`, `kern.sysv.msgmax`, `kern.sysv.msgmnb`, `kern.sysv.msgmni`, `kern.sysv.semmns`, and `kern.sysv.semmsl`. Shared memory segment, message queue, semaphore set, and local socket enumeration report `not_supported`; public `libproc` socket records do not expose the kernel reference count required by the portable `local_socket` contract | Backend written from the public Darwin sysctl interface with synthetic System V IPC limits parsing and error contract tests; compiled and exercised on the recorded GitHub-hosted macOS runner with Apple Clang and the Xcode 26.6 SDK | Verified on the recorded GitHub-hosted macOS runner |

The Windows status reflects compilation and execution on the recorded host.

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
| Environment and paths | Broad | Broad | Broad | Broad | Broad | Partial | Restricted | Not supported |
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
   legacy, compatibility-runtime, and WebAssembly backends where the selected
   profile's language requirements and acceptable platform sources exist.
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
6. preserve the Hosted Full and Freestanding Minimal language and capability
   boundaries without weakening either profile's fallback contract.
