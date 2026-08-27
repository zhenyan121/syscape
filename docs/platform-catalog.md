# Syscape Platform, Architecture, and Toolchain Catalog

Last reviewed: 2026-08-24

## Purpose

This catalog records environments in which C++ code may run and that Syscape
may eventually recognize. It is intentionally broader than the implementation
roadmap in [support-matrix.md](support-matrix.md).

An entry in this catalog is not a support claim. An architecture backend in a
compiler, a C++ toolchain for a device, and a Syscape backend verified on that
device are three different things. Actual implementation and verification
status belongs in the support matrix.

## Classification Model

Each target is described along independent axes:

- **Environment:** operating system, compatibility runtime, sandbox, RTOS, or
  bare-metal execution.
- **Architecture:** the instruction-set and ABI family, independent of the
  operating system.
- **Toolchain:** the compiler and standard-library implementation. A toolchain
  is not itself an operating system.
- **Syscape profile:** the largest API profile that the environment can
  realistically support.
- **State:** whether Syscape has designed, implemented, and verified a backend.

The profiles are:

| Profile | Contract |
| --- | --- |
| Hosted Full | Strict C++17, the complete hosted standard library, and the full portable query API. |
| Sandboxed/Restricted | Hosted or hosted-like strict C++17 with only information allowed by the sandbox and public platform APIs. |
| RTOS/Constrained | A selectable runtime subset backed by public RTOS APIs and explicit board providers; headers declare requirements no lower than strict C++11. |
| Freestanding Minimal | Strict C++11 allocation-free compile-target, architecture, byte-order, toolchain, and board-capability information only. |
| SDK Restricted | Cataloged, but no implementation promise until the official SDK is legally available and real hardware can be used for verification. |

Platform, RTOS, and MCU backend entries are **Not started** unless a row
explicitly says **SDK restricted**. Foundation detection evidence and toolchain
identification may have a later state without implying a complete platform
backend.

## Desktop, Server, and General-Purpose Systems

| Platform family | Representative architectures | Representative C++ toolchains | Intended profile | State |
| --- | --- | --- | --- | --- |
| Windows | x86, x86-64, Arm64 | MSVC, Clang/clang-cl, GCC/MinGW-w64 | Hosted Full | Initial OS, CPU, memory, process, user, filesystem, network interface, unicast route, default-gateway, DNS-configuration, interface statistics, locale, environment, resource, power, storage, virtualization, GPU, display, security, sensor, audio, input, Windows 10 version 1803-or-later camera, Bluetooth, Wi-Fi, printer, process list, connection inventory, and software inventory backends implemented; SDK compilation and runtime unverified |
| Linux | x86, x86-64, Arm, Arm64, RISC-V, LoongArch, PowerPC, s390x, MIPS, and other maintained ports | GCC, Clang | Hosted Full | Initial OS, CPU, memory, process, user, filesystem, network interface, unicast route, default-gateway, DNS-configuration, interface statistics, locale, environment, resource, power, storage, virtualization, GPU, display, security, sensor, audio, input, camera, Bluetooth, Wi-Fi, printer, process list, connection inventory, and software inventory queries verified on Arch Linux x86-64; other targets remain unverified |
| macOS | x86-64, Arm64 | Apple Clang | Hosted Full | Initial OS, CPU, memory, process, user, filesystem, network interface, unicast route, default-gateway, DNS-configuration, interface statistics, locale, environment, resource, power, storage, virtualization, GPU, display, security, sensor, audio, input, camera, Bluetooth, printer, process list, connection inventory, and software inventory backends implemented; Wi-Fi uses the generic unsupported fallback; SDK compilation and runtime unverified |
| FreeBSD | x86, x86-64, Arm, Arm64, RISC-V, PowerPC, and supported release architectures | Clang, GCC | Hosted Full | Not started |
| OpenBSD | x86-64, Arm64, and supported release architectures | Clang, GCC | Hosted Full | Not started |
| NetBSD | x86, x86-64, Arm, Arm64, MIPS, PowerPC, SPARC, and other supported release architectures | GCC, Clang | Hosted Full | Not started |
| DragonFly BSD | x86-64 | GCC, Clang | Hosted Full | Not started |
| illumos and OpenIndiana | Primarily x86-64, plus ports maintained by each distribution | GCC, Clang | Hosted Full | Not started |
| Oracle Solaris | x86-64, SPARC | GCC, Oracle Developer Studio | Hosted Full | Not started |
| AIX | POWER | IBM XL/Open XL, GCC, Clang where available | Hosted Full | Not started |
| HP-UX | PA-RISC, Itanium | HP C++, GCC where available | Hosted Full | Not started |
| Haiku | x86, x86-64 | GCC | Hosted Full | Not started |
| SerenityOS | x86-64 and maintained ports | Clang | Hosted Full | Not started |
| Redox OS | x86-64 and maintained ports | LLVM-based and GCC-compatible cross-toolchains where available | Hosted Full | Not started |
| GNU/Hurd | Primarily x86 and x86-64 | GCC, Clang where available | Hosted Full | Not started |
| MINIX | Architectures supported by the selected release | Clang or GCC-based toolchains | Hosted Full | Not started |
| OpenVMS | x86-64, Itanium, Alpha depending on release | Vendor and ported C++ toolchains | Hosted Full | Not started |
| z/OS | IBM Z | IBM Open XL and supported vendor toolchains | Hosted Full | Not started |
| IBM i | POWER | IBM i C++ toolchains | Hosted Full | Not started |
| DOS and FreeDOS | x86 | DJGPP GCC, OpenWatcom | RTOS/Constrained | Not started |
| OS/2 and ArcaOS | x86 | GCC ports, OpenWatcom | Hosted Full | Not started |
| AmigaOS and MorphOS | m68k, PowerPC, and platform-specific ports | GCC-based and vendor toolchains | RTOS/Constrained | Not started |
| RISC OS | Arm | GCCSDK and other platform toolchains | RTOS/Constrained | Not started |

Release, architecture, and compiler availability changes over time. A row lists
catalog candidates, not a promise that every named combination provides the
strict language mode and library facilities required by its intended profile.

## Compatibility Environments and Product Variants

| Environment | Classification | Backend relationship | State |
| --- | --- | --- | --- |
| Cygwin | POSIX compatibility runtime on Windows | Separate compatibility behavior; may combine POSIX and Windows APIs | Not started |
| MinGW and MinGW-w64 | GCC/Clang Windows toolchain and runtime | Uses the Windows backend with toolchain-specific validation | Not started |
| MSYS2 | Development and package environment, commonly using MinGW-w64 or Cygwin-like runtimes | Classified by the produced executable's runtime, not by the shell used to build it | Not started |
| Windows Subsystem for Linux | Linux userspace hosted by Windows | Linux backend plus explicit WSL environment detection | Not started |
| Wine | Windows compatibility runtime on another host | Windows API backend with explicit compatibility-runtime detection | Not started |
| ChromeOS | Linux-based product with sandbox and device-policy restrictions | Linux-derived backend with runtime restrictions | Not started |
| SteamOS and Steam Deck | Linux distribution and x86-64 device family | Linux backend; Steam Deck is not a separate operating-system backend | Not started |

## Mobile and Consumer Platforms

| Platform family | Representative architectures | Intended profile | State |
| --- | --- | --- | --- |
| Android | Arm, Arm64, x86, x86-64 | Sandboxed/Restricted | Not started |
| iOS | Arm64 | Sandboxed/Restricted | Not started |
| iPadOS | Arm64 | Sandboxed/Restricted | Not started |
| visionOS | Arm64 | Sandboxed/Restricted | Not started |
| watchOS | Arm64-family Apple targets | Sandboxed/Restricted | Not started |
| tvOS | Arm64 | Sandboxed/Restricted | Not started |
| HarmonyOS and OpenHarmony | Arm, Arm64, x86-family, and supported device ports | Sandboxed/Restricted or RTOS/Constrained depending on product | Not started |
| KaiOS | Product-dependent Arm targets using a web-oriented Linux-derived stack | Sandboxed/Restricted | Not started |
| Tizen | Arm, Arm64, x86-family depending on product | Sandboxed/Restricted | Not started |
| Sailfish OS | Arm and x86-family depending on product | Sandboxed/Restricted | Not started |

## WebAssembly Environments

| Environment | Host model | Intended profile | State |
| --- | --- | --- | --- |
| Browser WebAssembly | Chrome, Firefox, Safari, Edge, and other standards-compliant browser hosts | Sandboxed/Restricted | Not started |
| Emscripten | C++-to-WebAssembly toolchain and browser/runtime compatibility layer | Sandboxed/Restricted | Not started |
| WASI | Capability-oriented non-browser WebAssembly system interface | Sandboxed/Restricted | Not started |
| Embedded WebAssembly runtimes | Runtime-specific host functions on devices and RTOS environments | RTOS/Constrained or Freestanding Minimal | Not started |

WebAssembly is an execution target, not an x86 or Arm architecture. The native
architecture beneath a WebAssembly runtime is intentionally hidden unless the
host explicitly exposes it.

## SDK-Restricted Game Consoles

| Platform family | Public classification | State |
| --- | --- | --- |
| PlayStation 4 | Proprietary console SDK and hardware | SDK restricted |
| PlayStation 5 | Proprietary console SDK and hardware | SDK restricted |
| Xbox One | Proprietary console SDK and hardware | SDK restricted |
| Xbox Series X/S | Proprietary console SDK and hardware | SDK restricted |
| Nintendo Switch | Proprietary console SDK and hardware | SDK restricted |
| Nintendo Switch 2 | Proprietary console SDK and hardware | SDK restricted |

No backend may be claimed for these systems based on unofficial headers,
reverse engineering, leaked material, or assumptions from related desktop
hardware. Implementation requires lawful SDK access and verification permitted
by the applicable platform terms.

## RTOS and Real-Time Environments

| RTOS family | Intended integration | Intended profile | State |
| --- | --- | --- | --- |
| FreeRTOS | Public kernel APIs plus an explicit board or vendor provider | RTOS/Constrained | Not started |
| Zephyr | Public kernel, device, and subsystem APIs enabled by the application configuration | RTOS/Constrained | Not started |
| QNX Neutrino | POSIX and documented QNX system APIs | Hosted Full or RTOS/Constrained | Not started |
| VxWorks | POSIX and documented VxWorks APIs available to the application | Hosted Full or RTOS/Constrained | Not started |
| RTEMS | POSIX and documented RTEMS APIs plus board provider data | RTOS/Constrained | Not started |
| ThreadX / Eclipse ThreadX | Public RTOS APIs plus an explicit board provider | RTOS/Constrained | Not started |
| NuttX | POSIX-like and documented NuttX APIs | RTOS/Constrained | Not started |
| embOS | Documented embOS APIs plus an explicit board provider | RTOS/Constrained | Not started |
| uC/OS | Documented uC/OS APIs plus an explicit board provider | RTOS/Constrained | Not started |
| INTEGRITY | Documented public SDK APIs available to the application | RTOS/Constrained | Not started |
| T-Kernel | Documented T-Kernel APIs plus an explicit board provider | RTOS/Constrained | Not started |
| ChibiOS | Public RTOS and hardware-abstraction APIs | RTOS/Constrained | Not started |
| Apache Mynewt | Public OS APIs plus an explicit board provider | RTOS/Constrained | Not started |
| Mbed OS | Public OS and target APIs | RTOS/Constrained | Not started |
| RIOT | Public OS APIs and documented C++ interoperability where available | RTOS/Constrained | Not started |

An RTOS name does not imply that every configuration enables conforming C++11,
the same standard-library facilities, networking, a filesystem, dynamic
allocation, or device discovery. Each backend and header must state its
requirements.

## MCU, SoC, and Board Families

These are hardware or framework families, not operating systems. Runtime
information beyond compile-target facts must come from a documented vendor SDK,
RTOS interface, or explicit application-supplied provider.

| Family | Representative devices or groups | Typical architecture family | State |
| --- | --- | --- | --- |
| Arduino ecosystem | Boards based on AVR, Arm, ESP, Renesas, and other MCUs | Board-dependent | Not started |
| Microchip AVR | ATmega328P, ATmega2560, ATtiny families | AVR | Not started |
| Microchip SAM | SAMD21, SAMD51, and related families | Arm | Not started |
| Espressif ESP | ESP8266, ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2 | Xtensa or RISC-V | Not started |
| STMicroelectronics STM32 | F0, F1, F2, F3, F4, F7, G0, G4, H5, H7, L0, L4, L5, U5, and later maintained families | Arm | Not started |
| Raspberry Pi microcontrollers | RP2040 and RP2350, including Raspberry Pi Pico boards | Arm and RISC-V-capable variants as documented by the vendor | Not started |
| Nordic Semiconductor | nRF51, nRF52, nRF53, nRF54 families | Arm | Not started |
| Texas Instruments | MSP430, TMS320, Sitara, SimpleLink, and supported MCU/DSP families | MSP430, DSP, Arm, and other vendor architectures | Not started |
| NXP | LPC, i.MX RT, Kinetis, and supported MCU/SoC families | Primarily Arm | Not started |
| Renesas | RX, RA, RZ, and supported MCU/SoC families | RX, Arm, and other vendor architectures | Not started |
| Microchip PIC | PIC32 and supported 32-bit MCU families | MIPS or Arm depending on device | Not started |
| SiFive and other RISC-V MCUs | Publicly documented RISC-V microcontrollers and SoCs | RISC-V | Not started |
| GigaDevice | GD32 MCU families | Arm or RISC-V depending on device | Not started |
| WCH | CH32 MCU families | RISC-V or Arm depending on device | Not started |
| Bouffalo Lab | BL602, BL616, and maintained BL-series devices | RISC-V and vendor-documented cores | Not started |

Specific part numbers are examples. The catalog is maintained at family level
so that a new package, memory size, or pin-compatible model does not require a
new Syscape platform classification.

## Architecture Catalog

Architecture recognition is separate from operating-system support. Detection
must report an explicit `unknown` value for an unrecognized target and must not
assume the runtime host matches a cross-compilation target.

### Mainstream general-purpose families

- x86 and x86-64;
- Arm and AArch64/Arm64;
- RISC-V, including 32-bit and 64-bit profiles;
- MIPS and MIPS64;
- PowerPC, PowerPC64, and POWER;
- SPARC and SPARC64;
- IBM System Z, s390, and s390x; and
- LoongArch.

### Embedded and specialized families

- AVR, MSP430, Xtensa, ARC, and C-SKY/CSKY;
- MicroBlaze and Nios II;
- SuperH/SH, V850, RL78, RX, and H8/300;
- Blackfin, CRIS, and TI PRU;
- TMS320 and other documented DSP targets; and
- vendor-specific Arm and RISC-V microcontroller profiles.

### Legacy, discontinued, and research candidates

- IA-64/Itanium, Alpha, Motorola 68000/m68k, and PA-RISC;
- PDP-11 and VAX;
- M32C, M32R, FR30, and FR-V;
- MMIX, Stormy16, Visium, and Epiphany;
- IQ2000, LM32, MeP, MCore, MN10300, Moxie, and NDS32/ND32; and
- any additional target for which a strict C++11-or-later compiler can build
  the relevant Syscape profile.

Legacy and research entries are detection candidates only. Their presence does
not claim that a current compiler, conforming C++11 language mode, required
standard library, or physical verification system is available. C++03 and
earlier toolchains may be cataloged, but they are outside Syscape's compilation
contract.

## Toolchain Catalog

| Toolchain family | Representative variants | Catalog role | State |
| --- | --- | --- | --- |
| GCC | Native GCC, cross GCC, MinGW-w64 GCC, DJGPP, and vendor-packaged GCC | Hosted, RTOS, and bare-metal candidate | Detection implemented; GCC 16.2.1 verified on Linux x86-64 |
| LLVM/Clang | Upstream Clang, clang-cl, cross Clang, and vendor distributions | Hosted, RTOS, WebAssembly, and bare-metal candidate | Detection implemented; Clang 22.1.8 verified on Linux x86-64 |
| Apple Clang | Xcode platform toolchains | Apple hosted and sandboxed platforms | Not started |
| Microsoft Visual C++ | MSVC and Windows SDK toolsets | Windows Hosted Full | Not started |
| Emscripten | Clang-based WebAssembly toolchain | Browser and supported WebAssembly runtimes | Not started |
| IBM XL and Open XL | AIX, Linux on POWER, and IBM platform variants | IBM hosted platforms | Not started |
| Oracle Developer Studio | Solaris toolchains | Solaris Hosted Full candidate | Not started |
| HP C++ | HP-UX platform toolchains | HP-UX legacy candidate | Not started |
| OpenWatcom | DOS, Windows, and OS/2 targets | Legacy and constrained candidate | Not started |
| Arm Compiler and Keil | Arm hosted and microcontroller toolchains | RTOS and bare-metal candidate | Not started |
| IAR C/C++ | Vendor-supported embedded targets | RTOS and bare-metal candidate | Not started |
| Green Hills | INTEGRITY and supported embedded targets | RTOS and SDK-restricted candidate | Not started |
| Texas Instruments | TI MCU and DSP toolchains | RTOS and bare-metal candidate | Not started |
| Renesas | RX, RA, and related toolchains | RTOS and bare-metal candidate | Not started |
| Microchip XC | PIC and AVR-family toolchains | RTOS and bare-metal candidate | Not started |
| Espressif toolchains | Xtensa and RISC-V GCC/Clang-based SDK toolchains | RTOS and bare-metal candidate | Not started |

The library remains zero-dependency when it supports a toolchain: compilers,
standard libraries, operating-system SDKs, and vendor SDKs are build
environments, not bundled library dependencies. Syscape source must use the
strict standard declared by each header—C++11 at minimum and C++17 for Hosted
Full—and must not require a compiler language extension.

## Evidence and Status Rules

The following evidence levels must not be conflated:

1. **Cataloged:** a target or toolchain is known and classified here.
2. **Compiles:** the applicable public headers compile and link for a concrete
   target triple and documented configuration.
3. **Implemented:** a Syscape backend uses acceptable platform sources and has
   complete host-independent tests.
4. **Verified:** the backend has executed successfully on the real named target
   and its failure paths and limitations are recorded.

Cross-compilation and emulation may establish **Compiles**, but only execution
on the real target establishes **Verified**. A compiler backend alone
establishes neither state for Syscape.

### Current foundation evidence

| Target fact | Verified environment | State |
| --- | --- | --- |
| x86-64 architecture, LP64 data model, little-endian byte order | Arch Linux, Linux 7.1.8, glibc 2.44, GCC 16.2.1 and Clang 22.1.8 | Verified |
| Linux operating-system and hosted execution classification | Arch Linux, Linux 7.1.8, x86-64 | Verified |
| GCC and Clang compiler plus libstdc++ identification | GCC 16.2.1 and Clang 22.1.8 on `x86_64-pc-linux-gnu` | Verified |
| Generic and unknown target fallbacks | Forced test configuration on Linux x86-64 | Verified |
| Freestanding Minimal header set | `x86_64-pc-linux-gnu` in strict C++11, C++14, and C++17 modes, including C++11 with `-ffreestanding`, under GCC 16.2.1 and Clang 22.1.8 | Compiles only |

All other architectures, standard libraries, compilers, operating systems,
RTOS families, MCU families, and SDK-restricted platforms remain unverified.

## Reference Policies

- [LLVM Community Support Policy](https://llvm.org/docs/SupportPolicy.html)
  distinguishes regularly tested core components from peripheral and
  experimental components. Syscape similarly records evidence rather than
  assuming that the presence of a backend is a verification guarantee.
- [GCC machine-dependent option documentation](https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html)
  demonstrates that architecture options depend on the configured target.
- [Zephyr C++ language support documentation](https://docs.zephyrproject.org/latest/develop/languages/cpp/index.html)
  documents configuration, toolchain, and standard-library limitations that a
  Syscape backend must record rather than hide.

## Updating This Catalog

- Add operating systems, runtimes, architectures, and toolchains to their own
  sections instead of combining them into one platform name.
- Keep MCU entries at family level and list individual devices only as useful
  examples.
- Update [support-matrix.md](support-matrix.md) when an entry moves beyond
  cataloging into design, implementation, compilation, or verification.
- Record the exact OS or RTOS version, architecture, ABI, toolchain, standard
  library, SDK configuration, board, and hardware used for verification.
- Do not remove a legacy target merely because a current development machine
  cannot test it; mark its real evidence level and limitations instead.
