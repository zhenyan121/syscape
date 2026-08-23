# Syscape Support Matrix and Information Catalog

Last reviewed: 2026-08-23

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
| Linux | glibc and musl; x86, Arm, and RISC-V | POSIX APIs, Linux system calls, procfs, sysfs, netlink, and documented kernel interfaces | In progress; OS and initial CPU identity/topology queries verified on one host |
| Windows | Supported Windows client and server releases; x86, x64, and Arm64 | Win32, NT-supported public APIs, IP Helper, SetupAPI, registry APIs, and other Windows SDK facilities | In progress; OS and initial CPU topology backends implemented but unverified |
| macOS | Supported macOS releases; x86-64 and Apple silicon | POSIX, `sysctl`, IOKit, SystemConfiguration, and other public system frameworks callable from C++ | In progress; OS and initial CPU-count backends implemented but unverified |

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
| 1 | `os.hpp` | Product name, version, build, kernel name and version, host name, boot time, uptime, and boot identifier where available | Implemented; Linux verified, Windows and macOS unverified |
| 1 | `cpu.hpp` | Architecture, vendor, model, packages, physical and logical cores, topology, caches, instruction-set features, frequency, affinity, and utilization | In progress; vendor identifiers, model labels, online logical, physical-core, and package counts, recorded and current clock frequencies, and cumulative system-wide utilization implemented where documented sources exist. Cache topology and instruction-set features remain not started. Affinity of the calling context belongs to the process module's scheduling slice rather than being duplicated here |
| 1 | `memory.hpp` | Physical memory, available memory, committed memory, swap or pagefile, page size, huge pages, pressure, and system utilization | In progress; page size, physical memory, the available-memory estimate, and swap or pagefile usage implemented where documented sources exist |
| 1 | `process.hpp` | Current process identity, parent, executable, command line, working directory, start time, CPU time, memory use, priority, affinity, threads, and resource limits | Implemented; Linux verified, Windows and macOS unverified |
| 1 | `user.hpp` | Current user identity, numeric or textual IDs, groups, home directory, shell, elevation, and login session | Implemented; Linux verified, Windows and macOS unverified. Login-session metadata beyond the recorded session name, and fine-grained capability or per-privilege grants beyond the privilege classification, remain not started |
| 1 | `filesystem.hpp` | Mounts, volumes, filesystem type, capacity, free space, block size, read-only state, and path limits | In progress; mounted-filesystem enumeration plus per-path capacity, free, available, block-size, and read-only queries implemented where documented platform sources exist. Path limits and volume metadata beyond mount-table entries remain not started |
| 1 | `network.hpp` | Network interfaces, addresses, prefix lengths, MAC addresses, MTU, state, routes, default gateways, DNS configuration, and host/domain names | In progress; interface enumeration with names, indices, operational state, loopback classification, link-layer addresses, and unicast IPv4/IPv6 addresses with prefix lengths implemented where documented platform sources exist. MTU, routes, default gateways, DNS configuration, host and domain names, and IPv6 zone identifiers remain not started |
| 1 | `locale.hpp` | Locale, preferred languages, country or region, text encoding, time zone, and UTC offset | In progress; the current locale identifier, the non-Unicode text-encoding label, and the current UTC offset implemented where documented platform sources exist. Preferred languages, country or region, and time-zone identifiers or display names remain not started |
| 2 | `storage.hpp` | Physical drives, partitions, bus and media type, model, firmware, capacity, logical and physical sector sizes, rotational state, removable state, and health data exposed by the OS | Not started |
| 2 | `power.hpp` | Batteries, charge, health, charging state, power source, estimated remaining time, and system power capabilities | Not started |
| 2 | `hardware.hpp` | System manufacturer and model, chassis, motherboard, firmware or BIOS, hardware UUID, and documented device inventory | Not started |
| 2 | `display.hpp` | Displays, bounds, resolution, work area, refresh rate, scale, orientation, color depth, and connection state | Not started |
| 2 | `gpu.hpp` | GPU name, vendor, device identity, driver, memory values exposed by the OS, and active adapter state | Not started |
| 2 | `virtualization.hpp` | Hypervisor presence and vendor, virtual machine hints, containers, namespaces, cgroups, jails, zones, WSL, and application sandboxing | Not started |
| 2 | `environment.hpp` | Process environment snapshot, temporary and configuration directories, terminal presence, and runtime environment characteristics | In progress; environment-variable lookup, standard user directories, and standard-stream terminal status implemented. Linux verified; Windows and macOS unverified. Environment snapshots and broader runtime characteristics remain not started |
| 2 | `resource.hpp` | System load, scheduler information, system-wide process and thread counts, open-file and open-handle totals, handle or file descriptor limits, and other capacity limits | Implemented; Linux verified, Windows and macOS unverified |
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

### CPU identity and online-topology evidence

This first CPU slice exposes distinct platform-provided vendor identifiers and
model labels plus system-wide counts of online logical processors, physical
cores, and CPU packages. The counts intentionally do not apply process affinity
or container CPU quotas. Cache topology, instruction-set features,
frequencies, and utilization are covered by the slice below; per-caller
affinity is covered by the process scheduling-priority slice.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every CPU query | Forced-backend standalone, runtime, C++11-rejection, and ODR tests under GCC and Clang | Verified |
| Linux | `_SC_NPROCESSORS_ONLN`, architecture-specific `/proc/cpuinfo` identifiers and labels, `/sys/devices/system/cpu/online`, and per-CPU sysfs package/core IDs; identity queries may return `not_found` on architectures that expose no recognized field | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44; strict C++17 GCC 16.2.1 and Clang 22.1.8; parser, malformed-data, UTF-8, topology-invariant, standalone, runtime, ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests | Verified for this slice on the listed host |
| Windows | `GetActiveProcessorCount` and `GetLogicalProcessorInformationEx`; vendor identifiers and model labels return `not_supported`; requires Windows 7 or later processor-group declarations | Backend implemented from public Windows APIs; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | Documented `hw.logicalcpu` and `hw.physicalcpu` `sysctlbyname` values; package count, vendor identifiers, and model labels return `not_supported` | Backend implemented from public Apple APIs; no Apple SDK or runtime available in the current environment | In progress; uncompiled and unverified |

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
| Windows | [`CallNtPowerInformation(ProcessorInformation)`](https://learn.microsoft.com/en-us/windows/win32/api/powerbase/nf-powerbase-callntpowerinformation) records supply CurrentMhz and MaxMhz multiplied exactly by one thousand; requires linking PowrProf.lib, which the public header documents; the records expose no minimum operating frequency, so that query returns `not_supported`; its documented sizing procedure and [`GetSystemTimes`](https://learn.microsoft.com/en-us/windows/win32/api/proctimeapi/nf-proctimeapi-getsystemtimes) are group-relative on systems with more than one processor group, so frequency and utilization queries report `not_supported` there instead of silently partial coverage; `STATUS_ACCESS_DENIED` maps to `permission_denied` while other failing NTSTATUS values map to `io_error`; GetSystemTimes supplies cumulative idle, kernel, and user totals in hundred-nanosecond units, where system ticks are the kernel total minus idle because the kernel total includes idle, and a kernel total below idle is malformed platform data | Backend written from public Microsoft APIs with synthetic record-conversion, zero-frequency, null-buffer, NTSTATUS mapping, kernel-below-idle, group-count gate, and live monotonicity tests; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | Documented `hw.cpufrequency_min` and `hw.cpufrequency_max` sysctl values converted from hertz with sub-kilohertz truncation; those keys are absent on Apple silicon, where both bound queries report `not_supported`; Darwin documents no public source for instantaneous clocks, so current frequencies return `not_supported`; utilization sums the documented [`host_processor_info`](https://developer.apple.com/documentation/kernel/1439494-host_processor_info) `PROCESSOR_CPU_LOAD_INFO` unsigned tick records, folding user plus nice into the user bucket, with the Mach port owned by an internal RAII guard and the implicit output buffer released through `vm_deallocate`; the underlying natural_t counters can wrap, so a caller must discard any interval whose later aggregate bucket is smaller; a failing kern_return_t maps to `io_error` | Backend written from public Apple APIs with synthetic per-processor tick-summing, high-bit-counter, and empty-input tests; no Apple runtime available in the current environment | In progress; uncompiled and unverified |

Windows and macOS statuses must not advance until their headers compile with
the official SDK and the tests execute on the real operating systems.

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
| Windows | `GlobalMemoryStatusEx` physical fields; `GetSystemInfo` page size; paging space returns `not_supported` because the pagefile fields describe commit limits scoped to the system or current process and cannot express paging-space capacity | Backend implemented from public Windows APIs; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | `hw.memsize`; free and inactive pages from `host_statistics64` (volatile purgeable pages already reside in the kernel's inactive population); binary `xsw_usage` from `vm.swapusage`; Mach failures map to `io_error` because kern_return_t has no standard category; host send rights are owned by an internal RAII guard | Backend implemented from public Apple APIs; no Apple SDK or runtime available in the current environment | In progress; uncompiled and unverified |

Windows and macOS statuses must not advance until their headers compile with
the official SDK and the tests execute on the real operating system.

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
| Windows | `GetCurrentProcessId`; parent ID by scanning a `CreateToolhelp32Snapshot` process snapshot with `Process32FirstW`/`Process32NextW` (snapshot enumeration can be expensive); `GetModuleFileNameW(NULL)`; `GetCommandLineW` plus `CommandLineToArgvW` with `LocalFree`; two-stage `GetCurrentDirectoryW`; UTF-16 to UTF-8 conversion at the boundary | Backend written from public Microsoft APIs; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | POSIX `getpid` and `getppid`; `_NSGetExecutablePath` reported verbatim and not canonicalized; `_NSGetArgc`/`_NSGetArgv`; POSIX `getcwd` | Backend written from public Darwin APIs; no Apple SDK or runtime available in the current environment | In progress; uncompiled and unverified |

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
| Windows | `GetProcessTimes` creation, user, and system values converted from the documented 1601-epoch hundred-nanosecond FILETIME form with pre-epoch rejection; working set from the documented [`PROCESS_MEMORY_COUNTERS`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters) query declared in `<psapi.h>` (kernel32 mapping on Windows 7 or later SDKs or Psapi.lib linkage); address-space extent from a documented [`VirtualQuery`](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualquery) walk over reserved and committed regions behind an injectable seam covered by synthetic region chains; thread count from a documented [`TH32CS_SNAPTHREAD`](https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-createtoolhelp32snapshot) snapshot filtered by owning process, which can be comparatively expensive on systems with many threads | Backend written from public Microsoft APIs with FILETIME-conversion, epoch-boundary, region-walk, malformed-region, native-error, and overflow synthetic tests; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | Documented `proc_pidinfo` `PROC_PIDTASKINFO` task fields supply nanosecond execution totals, resident and virtual sizes, and the thread count; the documented `KERN_PROC` sysctl supplies `p_starttime` for the recorded creation instant; microsecond components outside one second are malformed platform data | Backend written from public Apple APIs with duration-limit, time-pair, and malformed-component synthetic tests; no Apple runtime available in the current environment | In progress; uncompiled and unverified |

Windows and macOS statuses must not advance until their headers compile with
the official SDK and the tests execute on the real operating systems.

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
| Windows | `GetPriorityClass` mapped onto the documented base-priority table from 4 (idle) through 24 (realtime) with unrecognized classes rejected as malformed data; `GetProcessAffinityMask` expanded only after `GetActiveProcessorGroupCount` establishes that group-relative bit positions are unambiguous system-wide indices, while multiple-group systems report `not_supported`; the process mask is validated as a subset of the system mask behind injectable validation and expansion seams; every resource-limit kind returns `not_supported` because the platform exposes no per-process equivalent; `_WIN32_WINNT` and `WINVER` values below 0x0601 are rejected, while absent values are scoped to 0x0601 only across the internal Windows SDK include boundary | Backend written from public Windows 7 or later APIs with synthetic class-mapping, group-count, and mask-expansion tests; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | POSIX `getpriority(PRIO_PROCESS)` validated against the Darwin-documented nice range from -20 through 20; no documented affinity source, so affinity returns `not_supported`; Darwin's signed 64-bit `rlim_t` uses the positive `RLIM_INFINITY` value 2^63−1 | Backend written from public POSIX interfaces with synthetic range, marker, and ordering tests; no Apple runtime available in the current environment | In progress; uncompiled and unverified |

Windows and macOS statuses must not advance until their headers compile with
the official SDK and the tests execute on the real operating systems.

### User identity evidence

The user module exposes two slices. The first slice exposes the real and
effective user and group identifiers plus the login name, home directory, and
recorded shell of the effective user. The second slice exposes the recorded
supplementary group set, a privilege classification of the effective
identity, and the login name of the controlling-terminal session. Numeric
identifiers are operating-system-scoped values; zero is valid where the
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
unprivileged rather than a fabricated elevated state. Session metadata beyond
the recorded session name, such as terminal devices, timestamps, and remote
hosts, remains not started.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every user query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | POSIX `getuid`, `geteuid`, `getgid`, and `getegid`; POSIX [`getpwuid_r`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/getpwuid_r.html) for the effective user supplies name, absolute home directory, and recorded shell; POSIX [`getgroups`](https://www.man7.org/linux/man-pages/man3/getgroups.3.html) supplies the supplementary set with insufficient-buffer retries capped before unbounded growth and reported ascending and unique because platforms document neither ordering nor uniqueness; privilege derives from `geteuid` alone; POSIX [`getlogin_r`](https://www.man7.org/linux/man-pages/man3/getlogin_r.3.html) supplies the controlling-terminal session name, whose absence conditions (`ENXIO`, `ENOTTY`, `ENOENT`) report `not_found` while other failures preserve their native codes; each textual query validates only its own field, so an unusable field cannot fail unrelated queries; an empty recorded shell is valid data while an empty session recording is malformed platform data; entries without a usable name or with a relative home directory are malformed platform data | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors` and high-signal warnings; buffer-growth (`ERANGE`) retry, permanently-insufficient-buffer, absent-entry, native-failure, malformed-entry, field-independence, group-normalization (unsorted, duplicated, empty), login-failure mapping, UTF-8 boundary, live-query identifier, group, and session cross-checks that tolerate hosts without a controlling terminal, forced fallback, C++11 rejection, standalone repeated inclusion, AddressSanitizer, UndefinedBehaviorSanitizer, and two-translation-unit ODR tests passed | Verified for this slice on the listed host |
| Windows | `GetUserNameW` for the login name and `SHGetKnownFolderPath(FOLDERID_Profile)` for the profile directory, converted from UTF-16 at the boundary; [`OpenProcessToken`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-openprocesstoken) with [`GetTokenInformation(TokenElevation)`](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-gettokeninformation) classifies privilege, requires linking the Advapi32 import library, preserves native system errors including access-denied conditions, and reports a size mismatch as malformed data; Win32-facility HRESULT failures are narrowed to their Win32 code under the system category and other HRESULTs preserve their full value under an internal category, keeping permission failures comparable; all four numeric identifiers and the supplementary groups return `not_supported` because security identifiers are structured platform values rather than portable numbers; the login-session name returns `not_supported` because Windows exposes no acceptable source recorded separately from the process account | Backend written from public Microsoft APIs with synthetic invalid-token-handle failure-path tests; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | The shared POSIX backend: `getuid`, `geteuid`, `getgid`, `getegid`, `getpwuid_r`, `getgroups`, and `getlogin_r` | Backend written from public POSIX interfaces; no Apple runtime available in the current environment | In progress; uncompiled and unverified |

User names, home directories, and login-session names can identify persons
or accounts. The queries are explicit, preserve permission errors, and
perform no logging, persistence, telemetry, or network access. Windows and
macOS statuses must not advance until their headers compile with the official
SDK and the tests execute on the real operating systems.

### Filesystem evidence

This first filesystem slice exposes the platform's mounted-filesystem table
and per-path volume capacity: total capacity, free capacity, the operating
system's unprivately-available estimate, allocation granularity, and
read-only state, all in bytes at the moment of the query. POSIX enumeration
reads only the mount table and never touches the listed volumes, so
unresponsive network filesystems cannot block it; Windows enumeration queries
each drive-letter volume and can block on unready media or unresponsive mapped
shares. Path limits, volume labels, drive and media types, and per-volume
file-system-type lookup remain not started.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for valid filesystem queries; common public-boundary validation still rejects empty, embedded-null, and invalid-UTF-8 paths before backend selection | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Kernel-documented `/proc/self/mounts` with its four documented octal escapes decoded; POSIX `statvfs` for capacity with `f_frsize` block sizes; undocumented escape codes, truncated records, and a zero effective block size are malformed platform data; byte totals beyond 64 bits report `value_too_large`; missing paths preserve native errno | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic-record parsing (escapes, malformed lines, extra fields), statvfs scaling including `f_bsize` fallback, overflow, boundary validation, live enumeration cross-checked against an independent `getmntent` decode of the same table, live `statvfs` cross-checks, native-error, invalid-argument, invalid-encoding, forced-fallback, C++11-rejection, AddressSanitizer, UndefinedBehaviorSanitizer, repeated-inclusion, and two-translation-unit ODR tests passed | Verified for this slice on the listed host |
| Windows | `GetLogicalDrives` plus `QueryDosDeviceW` for sources, `GetVolumeInformationW` for file-system names and the read-only flag, `GetFileAttributesW` for preserving missing-path failures, `GetDiskFreeSpaceExW` for capacity, `GetVolumePathNameW` for resolving files, directories, junction points, and mounted folders to the volume actually holding the path, and `GetDiskFreeSpaceW` cluster size as the documented stand-in for a fundamental block size; only drive-letter volumes are enumerated, letters without ready media or locked volumes are omitted, device-mapping queries degrade to empty sources by design, and unlike POSIX enumeration the Windows enumeration contacts each drive-letter volume and may block on unready media or unresponsive network shares | Backend written from public Microsoft APIs with injectable enumeration seams covered by synthetic-data tests; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | Documented `getfsstat` interface into caller-owned buffers with growth retries, POSIX `statvfs` for capacity | Backend written from public Apple APIs with synthetic `statfs` conversion tests; no Apple runtime available in the current environment | In progress; uncompiled and unverified |

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
interface. Windows and macOS statuses must not advance until their headers
compile with the official SDK and the tests execute on the real operating
systems.

### Network interface evidence

This first network slice exposes the platform's network interfaces with
their names, indices, operational state, loopback classification, link-layer
addresses, and unicast IPv4/IPv6 addresses with prefix lengths, as observed
during the enumeration call. MTU, routes, default gateways, DNS
configuration, host and domain names, and IPv6 zone identifiers remain not
started. Interface rows of families outside this slice are skipped without
failing the query, and an interface without unicast addresses is valid data.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every network query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Documented [`getifaddrs`](https://www.man7.org/linux/man-pages/man3/getifaddrs.3.html) enumeration with POSIX `if_nametoindex` indices; documented [packet socket](https://www.man7.org/linux/man-pages/man7/packet.7.html) AF_PACKET rows supply link-layer addresses; a netmask with one bits after zero bits and an IPv4 row with a foreign netmask family are malformed platform data; a recorded hardware length beyond the eight-byte `sockaddr_ll` storage cannot be represented by this source and fails the snapshot with `not_supported` instead of being truncated; administratively up interfaces without running traffic report explicit unknown state | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic-chain grouping, ordering, contiguous-prefix, boundary-prefix, non-contiguous-mask, mask-family-mismatch, flags-only-row, unknown-family, hardware-length, index-resolution-failure, and boundary-validation tests; live enumeration cross-checked against an independent `getifaddrs` walk including loopback-address checks; forced-fallback, C++11-rejection, repeated-inclusion, two-translation-unit ODR, AddressSanitizer, and UndefinedBehaviorSanitizer tests passed | Verified for this slice on the listed host |
| Windows | Documented [`GetAdaptersAddresses`](https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses) enumeration with `GAA_FLAG_INCLUDE_PREFIX`, documented buffer-growth retries capped before unbounded growth, friendly-name UTF-16 to UTF-8 conversion with ANSI identifier fallback, IPv4 `IfIndex` with `Ipv6IfIndex` fallback for IPv6-only adapters, `OperStatus`, `IfType`, physical address, and `OnLinkPrefixLength` mapping, and injectable enumeration seams covered by synthetic-data tests; an adapter with both protocol indices zero cannot satisfy the portable nonzero-index contract and produces explicit `not_supported`; requires Windows Vista or later and linking the Iphlpapi import library, which the public header documents | Backend written from public Microsoft APIs; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | Shared POSIX `getifaddrs` backend reading Darwin's documented [`sockaddr_dl`](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/sockaddr_dl.3.html) AF_LINK rows, preserving link-layer addresses longer than six bytes through recorded length fields | Backend written from public Darwin APIs with synthetic AF_LINK conversion tests; no Apple runtime available in the current environment | In progress; uncompiled and unverified |

Windows and macOS statuses must not advance until their headers compile with
the official SDK and the tests execute on the real operating systems.

### Locale identity and UTC-offset evidence

This first locale slice exposes the process's current locale identifier, the
label of the encoding used for non-Unicode narrow text, and the local time
zone's UTC offset in effect at the moment of the query, in seconds east of
UTC. Locale identifiers and encoding labels are reported verbatim and are not
normalized across platforms: POSIX systems report the C runtime's locale
string and LC_CTYPE codeset name, while Windows reports the Microsoft C
runtime's locale string and decimal current multibyte code-page identifier.
Preferred languages, country or region, and time-zone identifiers or display
names remain not started. These queries observe C-runtime locale and time-zone
state, so concurrent changes to that state are a documented unavoidable
platform race. The Microsoft C runtime may additionally configure locale state
per thread.

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every locale query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | The query form of POSIX [`setlocale`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/setlocale.html) for the locale identifier, POSIX [`nl_langinfo`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/nl_langinfo.html) `CODESET` for the encoding label, and `tzset` plus `localtime_r` with a C99 `strftime("%z")` rendering for the offset; an empty or undeterminable `%z` rendering reports `not_found`, and offset renderings outside the sign-plus-four-or-six-digit shape are malformed platform data | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; synthetic sign, digit-range, length, and extended-second offset-parsing tests, public-boundary empty, non-UTF-8, and offset-range tests, and live queries cross-checked against independent `setlocale`, `nl_langinfo`, and `sscanf`-decoded `strftime("%z")` references; forced-fallback, C++11-rejection, repeated-inclusion, and two-translation-unit ODR tests passed | Verified for this slice on the listed host |
| Windows | The query form of the Microsoft C runtime `setlocale` for the process locale, `_getmbcp` for the runtime's current multibyte code page rendered as a decimal label, and [`GetTimeZoneInformation`](https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-gettimezoneinformation) with the bias component reported active for the instant; `TIME_ZONE_ID_UNKNOWN` still carries a valid base bias, dynamic time zones resolve through the same interface, and a summed bias reaching one day is malformed platform data | Backend written from documented Microsoft APIs with synthetic code-page rendering, bias-combination, and full-width `LONG` overflow tests; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | Shared POSIX backend: `setlocale`, `nl_langinfo(CODESET)`, and `tzset` plus `localtime_r` with `strftime("%z")` | Backend written from public POSIX interfaces with synthetic parsing and boundary tests; no Apple runtime available in the current environment | In progress; uncompiled and unverified |

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
Windows and macOS statuses must not advance until their headers compile with
the official SDK and the tests execute on the real operating systems.

### Environment variables, standard directories, and terminal status evidence

This environment slice exposes environment variable lookup, existence checking,
standard filesystem directories (temporary directory, user home, user configuration,
user data, and user cache), and standard streams interactive terminal status
(stdin, stdout, stderr). All returned paths are guaranteed to be normalized
non-empty UTF-8 absolute paths without trailing directory separators (except
for the root directory).

| Backend | Data sources and limitations | Evidence | State |
| --- | --- | --- | --- |
| Generic Hosted Full fallback | Portable `not_supported` results for every environment query | Forced-backend standalone and runtime tests under GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux | Documented POSIX [`getenv`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/getenv.html) for variable retrieval; XDG Base Directory specification (`$XDG_CONFIG_HOME`, `$XDG_DATA_HOME`, `$XDG_CACHE_HOME`) with `$HOME` and shared reentrant `getpwuid_r` fallback; `$TMPDIR` with `/tmp` fallback; POSIX [`isatty`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/isatty.html) for terminal checks. Queries do not mutate the process environment; callers must serialize C or POSIX environment mutation against queries because those APIs provide no portable reader/writer synchronization contract | Arch Linux, Linux 7.1.8, x86-64, glibc 2.44. Strict C++17 GCC 16.2.1 and Clang 22.1.8 with `-pedantic-errors`, high-signal warnings, and `-Werror`; variable existence, non-existence, empty values, invalid names, invalid UTF-8 values, path normalization, trailing slash trimming, XDG directories, temporary directory, and terminal status tests; forced-fallback, C++11-rejection, repeated-inclusion, and two-translation-unit ODR tests passed | Verified for this slice on the listed host |
| Windows | Documented [`GetEnvironmentVariableW`](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getenvironmentvariablew) with UTF-16 to UTF-8 conversion; [`GetTempPathW`](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettemppathw) for temporary directory; [`SHGetKnownFolderPath`](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath) for `FOLDERID_RoamingAppData`, `FOLDERID_LocalAppData`, and `FOLDERID_Profile`; `_isatty` combined with [`GetConsoleMode`](https://learn.microsoft.com/en-us/windows/console/getconsolemode) on `STD_INPUT_HANDLE`, `STD_OUTPUT_HANDLE`, and `STD_ERROR_HANDLE` | Backend written from public Microsoft APIs; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | POSIX `getenv` with UTF-8 validation; `confstr(_CS_DARWIN_USER_TEMP_DIR)` with POSIX `/tmp` fallback; Standard macOS user directories (`~/Library/Application Support`, `~/Library/Caches`); POSIX `isatty` for terminal checks | Backend written from public Darwin and POSIX APIs; no Apple runtime available in the current environment | In progress; uncompiled and unverified |

Windows and macOS statuses must not advance until their headers compile with
the official SDK and the tests execute on the real operating systems.

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
| Windows | Documented [`GetPerformanceInfo`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getperformanceinfo) supplies ProcessCount, ThreadCount, and HandleCount from one call; requires Windows 7 or later SDK declarations with Kernel32.lib linkage or Psapi.lib; HandleCount counts every kernel-object handle kind rather than files only, so it backs the separate open_handle_count() query while open_file_count() returns `not_supported`; load averages return `not_supported` because processor-time counters measure interval utilization rather than damped demand, scheduling-entity counts return `not_supported`, and the descriptor-limit query returns `not_supported` because Windows documents no configurable system-wide handle limit; zero process or thread totals are malformed data because the calling process and thread must exist | Backend written from public Microsoft APIs with synthetic snapshot-validation tests; no Windows SDK or runtime available in the current environment | In progress; uncompiled and unverified |
| macOS | Documented `getloadavg` interface for the three samples; process counts enumerate the documented `KERN_PROC_ALL` table through `sysctl` with growth retries capped before unbounded retries; the thread count reads `kern.num_threads`, the open-file count reads `kern.num_files`, and the limit reads `kern.maxfiles`; those three long-stable XNU sysctls are absent from Apple's formal documentation set and are used only because the platform exposes no stronger documented source; scheduling-entity counts and open_handle_count() return `not_supported` because Darwin documents neither a scheduling-entity nor an all-handles total | Backend written from public Darwin interfaces with synthetic nonnegative-scalar validation tests; no Apple runtime available in the current environment | In progress; uncompiled and unverified |

The Linux sources follow the kernel's documented
[`proc`](https://docs.kernel.org/filesystems/proc.html) interface and the
[file-handle sysctl](https://docs.kernel.org/admin-guide/sysctl/fs.html)
documentation. The Windows implementation follows the public
[performance information](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getperformanceinfo)
API. The macOS enumeration follows Darwin's documented
[`sysctl`](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/sysctlbyname.3.html)
process-table interface. Windows and macOS statuses must not advance until
their headers compile with the official SDK and the tests execute on the real
operating systems.

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
