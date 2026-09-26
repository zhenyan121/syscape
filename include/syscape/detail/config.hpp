#ifndef SYSCAPE_DETAIL_CONFIG_HPP
#define SYSCAPE_DETAIL_CONFIG_HPP

#if (defined(__sun) || defined(__sun__)) && !defined(_POSIX_PTHREAD_SEMANTICS)
#define _POSIX_PTHREAD_SEMANTICS 1
#endif

#if defined(__OHOS__) || defined(__OpenHarmony__)
#define SYSCAPE_TARGET_OPENHARMONY 1
#endif

#if defined(__HAIKU__)
#define SYSCAPE_TARGET_HAIKU 1
#endif

#if (defined(_AIX) || defined(__TOS_AIX__)) && !defined(_PASE) &&              \
    !defined(__PASE__) && !defined(__OS400__) && !defined(__OS400_TGTVRM__) && \
    !defined(SYSCAPE_TARGET_IBMI)
#define SYSCAPE_TARGET_AIX 1
#if !defined(_BSD)
#define _BSD 44
#endif
#endif

#if defined(__hpux) || defined(__hpux__)
#define SYSCAPE_TARGET_HPUX 1
#if !defined(_XOPEN_SOURCE_EXTENDED)
#define _XOPEN_SOURCE_EXTENDED 1
#endif
#if !defined(_PSTAT64)
#define _PSTAT64 1
#endif
#endif

#if defined(__GNU__) && !defined(__linux__)
#define SYSCAPE_TARGET_HURD 1
#elif defined(HURD) || defined(SYSCAPE_TARGET_HURD)
#define SYSCAPE_TARGET_HURD 1
#endif

#if defined(__serenity__)
#define SYSCAPE_TARGET_SERENITY 1
#elif defined(SERENITY)
#define SYSCAPE_TARGET_SERENITY 1
#endif

#if defined(__redox__)
#define SYSCAPE_TARGET_REDOX 1
#elif defined(REDOX)
#define SYSCAPE_TARGET_REDOX 1
#endif

#if defined(__minix) || defined(__minix__)
#define SYSCAPE_TARGET_MINIX 1
#elif defined(MINIX) || defined(SYSCAPE_TARGET_MINIX)
#define SYSCAPE_TARGET_MINIX 1
#endif

#if defined(__QNXNTO__) || defined(__QNX__)
#define SYSCAPE_TARGET_QNX 1
#elif defined(QNX) || defined(SYSCAPE_TARGET_QNX)
#define SYSCAPE_TARGET_QNX 1
#endif

#if defined(__VXWORKS__) || defined(_WRS_KERNEL) || defined(__RTP__)
#define SYSCAPE_TARGET_VXWORKS 1
#elif defined(VXWORKS) || defined(SYSCAPE_TARGET_VXWORKS)
#define SYSCAPE_TARGET_VXWORKS 1
#endif

#if defined(SYSCAPE_TARGET_VXWORKS)
#if defined(_WRS_KERNEL)
#define SYSCAPE_TARGET_VXWORKS_KERNEL 1
#elif defined(__RTP__)
#define SYSCAPE_TARGET_VXWORKS_RTP 1
#else
// Default to RTP when neither _WRS_KERNEL nor __RTP__ is explicitly specified
// (e.g. host simulation checks or generic VxWorks userland)
#define SYSCAPE_TARGET_VXWORKS_RTP 1
#endif
#endif

#if defined(__rtems__)
#define SYSCAPE_TARGET_RTEMS 1
#elif defined(RTEMS) || defined(SYSCAPE_TARGET_RTEMS)
#define SYSCAPE_TARGET_RTEMS 1
#endif

#if defined(__ZEPHYR__)
#define SYSCAPE_TARGET_ZEPHYR 1
#elif defined(ZEPHYR) || defined(SYSCAPE_TARGET_ZEPHYR)
#define SYSCAPE_TARGET_ZEPHYR 1
#endif

#if defined(__NuttX__)
#define SYSCAPE_TARGET_NUTTX 1
#elif defined(NUTTX) || defined(SYSCAPE_TARGET_NUTTX)
#define SYSCAPE_TARGET_NUTTX 1
#endif

#if defined(__wasi__)
#define SYSCAPE_TARGET_WASI 1
#elif defined(WASI) || defined(SYSCAPE_TARGET_WASI)
#define SYSCAPE_TARGET_WASI 1
#endif

#if defined(__EMSCRIPTEN__)
#define SYSCAPE_TARGET_EMSCRIPTEN 1
#elif defined(EMSCRIPTEN) || defined(SYSCAPE_TARGET_EMSCRIPTEN)
#define SYSCAPE_TARGET_EMSCRIPTEN 1
#endif

#if defined(__Fuchsia__) || defined(FUCHSIA) || defined(SYSCAPE_TARGET_FUCHSIA)
#define SYSCAPE_TARGET_FUCHSIA 1
#endif

#if defined(FREERTOS) || defined(__FREERTOS__) ||                              \
    defined(SYSCAPE_TARGET_FREERTOS)
#define SYSCAPE_TARGET_FREERTOS 1
#endif

#if defined(THREADX) || defined(__THREADX__) || defined(TX_API_H) ||           \
    defined(SYSCAPE_TARGET_THREADX)
#define SYSCAPE_TARGET_THREADX 1
#endif

#if defined(EMBOS) || defined(__EMBOS__) || defined(RTOS_H) ||                 \
    defined(SYSCAPE_TARGET_EMBOS)
#define SYSCAPE_TARGET_EMBOS 1
#endif

#if defined(UCOS) || defined(__UCOS__) || defined(UCOS_II) ||                  \
    defined(__UCOS_II__) || defined(UCOS_III) || defined(__UCOS_III__) ||      \
    defined(OS_uCOS_II) || defined(OS_uCOS_III) ||                             \
    defined(SYSCAPE_TARGET_UCOS)
#define SYSCAPE_TARGET_UCOS 1
#endif

#if defined(__INTEGRITY) || defined(INTEGRITY) ||                              \
    defined(SYSCAPE_TARGET_INTEGRITY)
#define SYSCAPE_TARGET_INTEGRITY 1
#endif

#if defined(_TKERNEL_) || defined(_TKERNEL) || defined(__TKERNEL__) ||         \
    defined(__tkernel__) || defined(_uTKERNEL_) || defined(_UTKERNEL_) ||      \
    defined(__uTKERNEL__) || defined(__UTKERNEL__) ||                          \
    defined(SYSCAPE_TARGET_TKERNEL)
#define SYSCAPE_TARGET_TKERNEL 1
#endif

#if defined(__CHIBIOS__) || defined(CHIBIOS) || defined(__CHIBIOS_RT__) ||     \
    defined(__CHIBIOS_NIL__) || defined(CH_KERNEL_MAJOR) ||                    \
    defined(SYSCAPE_TARGET_CHIBIOS)
#define SYSCAPE_TARGET_CHIBIOS 1
#endif

#if defined(__MYNEWT__) || defined(MYNEWT) || defined(MYNEWT_VAL) ||           \
    defined(SYSCAPE_TARGET_MYNEWT)
#define SYSCAPE_TARGET_MYNEWT 1
#endif

#if defined(__MBED__) || defined(MBED) || defined(MBED_MAJOR_VERSION) ||       \
    defined(SYSCAPE_TARGET_MBED)
#define SYSCAPE_TARGET_MBED 1
#endif

#if defined(RIOT_VERSION) || defined(__RIOT__) || defined(RIOT) ||             \
    defined(SYSCAPE_TARGET_RIOT)
#define SYSCAPE_TARGET_RIOT 1
#endif

#if (defined(__CYGWIN__) || defined(CYGWIN) ||                                 \
     defined(SYSCAPE_TARGET_CYGWIN)) &&                                        \
    !defined(__MSYS__)
#define SYSCAPE_TARGET_CYGWIN 1
#endif

#if defined(__MSDOS__) || defined(MSDOS) || defined(_MSDOS) ||                 \
    defined(__DOS__) || defined(SYSCAPE_TARGET_DOS)
#define SYSCAPE_TARGET_DOS 1
#endif

#if defined(__OS2__) || defined(OS2) || defined(_OS2) ||                       \
    defined(SYSCAPE_TARGET_OS2)
#define SYSCAPE_TARGET_OS2 1
#endif

#if defined(__amigaos__) || defined(__AMIGA__) || defined(AMIGA) ||            \
    defined(__amigaos4__) || defined(__MORPHOS__) || defined(__morphos__) ||   \
    defined(SYSCAPE_TARGET_AMIGAOS)
#define SYSCAPE_TARGET_AMIGAOS 1
#endif

#if defined(__riscos__) || defined(__riscos) || defined(RISCOS) ||             \
    defined(__RISCOS__) || defined(SYSCAPE_TARGET_RISCOS)
#define SYSCAPE_TARGET_RISCOS 1
#endif

#if defined(__VMS) || defined(__VMS__) || defined(VMS) || defined(__vms) ||    \
    defined(__vms__) || defined(SYSCAPE_TARGET_OPENVMS)
#define SYSCAPE_TARGET_OPENVMS 1
#endif

#if defined(__MVS__) || defined(_MVS) || defined(__OS390__) ||                 \
    defined(__zos__) || defined(__TOS_MVS__) || defined(__TOS_OS390__) ||      \
    defined(SYSCAPE_TARGET_ZOS)
#define SYSCAPE_TARGET_ZOS 1
#endif

#if defined(__OS400__) || defined(__OS400_TGTVRM__) || defined(_PASE) ||       \
    defined(__PASE__) || defined(__ILEC400__) || defined(SYSCAPE_TARGET_IBMI)
#define SYSCAPE_TARGET_IBMI 1
#endif

#if defined(__TIZEN__) || defined(__tizen__) || defined(SYSCAPE_TARGET_TIZEN)
#define SYSCAPE_TARGET_TIZEN 1
#endif

#if defined(__SAILFISH__) || defined(__sailfish__) ||                          \
    defined(__sailfishos__) || defined(__SILICA__) ||                          \
    defined(SYSCAPE_TARGET_SAILFISH)
#define SYSCAPE_TARGET_SAILFISH 1
#endif

#if defined(__KAIOS__) || defined(__kaios__) || defined(__B2G__) ||            \
    defined(SYSCAPE_TARGET_KAIOS)
#define SYSCAPE_TARGET_KAIOS 1
#endif

#if defined(__AVR__) || defined(__AVR) || defined(ARDUINO_ARCH_AVR) ||         \
    defined(SYSCAPE_TARGET_MCU_AVR)
#define SYSCAPE_TARGET_MCU_AVR 1
#endif

#if defined(__SAMD21__) || defined(__SAMD51__) || defined(__SAM3X8E__) ||      \
    defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_SAM) ||                 \
    defined(__SAM__) || defined(SYSCAPE_TARGET_MCU_SAM)
#define SYSCAPE_TARGET_MCU_SAM 1
#endif

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32) ||                    \
    defined(ARDUINO_ARCH_ESP8266) || defined(ESP32) || defined(ESP8266) ||     \
    defined(SYSCAPE_TARGET_MCU_ESP)
#define SYSCAPE_TARGET_MCU_ESP 1
#endif

#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) ||                \
    defined(STM32F3) || defined(STM32F4) || defined(STM32F7) ||                \
    defined(STM32G0) || defined(STM32G4) || defined(STM32H5) ||                \
    defined(STM32H7) || defined(STM32L0) || defined(STM32L1) ||                \
    defined(STM32L4) || defined(STM32L5) || defined(STM32U5) ||                \
    defined(ARDUINO_ARCH_STM32) || defined(__STM32__) ||                       \
    defined(SYSCAPE_TARGET_MCU_STM32)
#define SYSCAPE_TARGET_MCU_STM32 1
#endif

#if defined(PICO_BOARD) || defined(PICO_RP2040) || defined(PICO_RP2350) ||     \
    defined(PICO_BUILD) || defined(ARDUINO_ARCH_RP2040) ||                     \
    defined(SYSCAPE_TARGET_MCU_RP)
#define SYSCAPE_TARGET_MCU_RP 1
#endif

#if defined(NRF51) || defined(NRF52) || defined(NRF53) || defined(NRF54) ||    \
    defined(NRF52840_XXAA) || defined(NRF52832_XXAA) ||                        \
    defined(ARDUINO_ARCH_NRF52) || defined(SYSCAPE_TARGET_MCU_NORDIC)
#define SYSCAPE_TARGET_MCU_NORDIC 1
#endif

#if defined(__MSP430__) || defined(__TMS320C28XX__) ||                         \
    defined(DEVICE_FAMILY_CC26X2) || defined(DEVICE_FAMILY_CC13X2) ||          \
    defined(SYSCAPE_TARGET_MCU_TI)
#define SYSCAPE_TARGET_MCU_TI 1
#endif

#if defined(CPU_LPC55S69JBD100) || defined(CPU_MIMXRT1062DVL6A) ||             \
    defined(__LPC17XX__) || defined(__LPC11XX__) ||                            \
    defined(SYSCAPE_TARGET_MCU_NXP)
#define SYSCAPE_TARGET_MCU_NXP 1
#endif

#if defined(__RX__) || defined(_RA_) || defined(ARDUINO_ARCH_RENESAS) ||       \
    defined(SYSCAPE_TARGET_MCU_RENESAS)
#define SYSCAPE_TARGET_MCU_RENESAS 1
#endif

#if defined(__PIC32MX__) || defined(__PIC32MZ__) || defined(__PIC32MK__) ||    \
    defined(__dsPIC33F__) || defined(__dsPIC33E__) ||                          \
    defined(SYSCAPE_TARGET_MCU_PIC)
#define SYSCAPE_TARGET_MCU_PIC 1
#endif

#if defined(GD32F10X) || defined(GD32F30X) || defined(GD32VF103) ||            \
    defined(SYSCAPE_TARGET_MCU_GD32)
#define SYSCAPE_TARGET_MCU_GD32 1
#endif

#if defined(CH32V003) || defined(CH32V103) || defined(CH32V203) ||             \
    defined(CH32V307) || defined(CH32F103) || defined(SYSCAPE_TARGET_MCU_CH32)
#define SYSCAPE_TARGET_MCU_CH32 1
#endif

#if defined(BL602) || defined(BL616) || defined(BL702) || defined(BL808) ||    \
    defined(SYSCAPE_TARGET_MCU_BOUFFALO)
#define SYSCAPE_TARGET_MCU_BOUFFALO 1
#endif

#if defined(SIFIVE_FE310) || defined(SIFIVE_U540) ||                           \
    defined(SYSCAPE_TARGET_MCU_SIFIVE)
#define SYSCAPE_TARGET_MCU_SIFIVE 1
#endif

// Defined when targeting an Arduino platform. When an Arduino build also
// defines underlying architecture macros (e.g. ARDUINO_ARCH_AVR,
// ARDUINO_ARCH_ESP32), both SYSCAPE_TARGET_MCU_ARDUINO and the specific
// MCU target macro will be defined; target_board_family() prioritizes
// the specific MCU architecture over the generic Arduino framework tag.
#if defined(ARDUINO) || defined(SYSCAPE_TARGET_MCU_ARDUINO)
#define SYSCAPE_TARGET_MCU_ARDUINO 1
#endif

#if defined(SYSCAPE_TARGET_ZEPHYR)
#if defined(CONFIG_POSIX_SINGLE_PROCESS)
#define SYSCAPE_ZEPHYR_HAS_POSIX_SINGLE_PROCESS 1
#endif
#if defined(CONFIG_POSIX_TIMERS)
#define SYSCAPE_ZEPHYR_HAS_POSIX_TIMERS 1
#endif
#endif

#if defined(__APPLE__) && defined(__MACH__)
#if defined(__has_include)
#if __has_include(<TargetConditionals.h>)
#include <TargetConditionals.h>
#endif
#endif
#if !defined(TARGET_OS_IPHONE)
#if defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) ||                 \
    defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) ||                  \
    defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) ||                     \
    defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#define TARGET_OS_IPHONE 1
#else
#define TARGET_OS_IPHONE 0
#endif
#endif
#if !defined(TARGET_OS_OSX)
#if TARGET_OS_IPHONE
#define TARGET_OS_OSX 0
#else
#define TARGET_OS_OSX 1
#endif
#endif
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define SYSCAPE_TARGET_APPLE_MOBILE 1
#else
#define SYSCAPE_TARGET_MACOS 1
#endif
#endif

#if defined(_MSVC_LANG)
#define SYSCAPE_DETAIL_CPLUSPLUS _MSVC_LANG
#else
#define SYSCAPE_DETAIL_CPLUSPLUS __cplusplus
#endif

#if SYSCAPE_DETAIL_CPLUSPLUS < 201103L
#error "Syscape requires C++11 or later"
#endif

#if SYSCAPE_DETAIL_CPLUSPLUS >= 201402L
#define SYSCAPE_DETAIL_CONSTEXPR14 constexpr
#else
#define SYSCAPE_DETAIL_CONSTEXPR14 inline
#endif

#endif
