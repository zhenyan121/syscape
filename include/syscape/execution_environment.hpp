#ifndef SYSCAPE_EXECUTION_ENVIRONMENT_HPP
#define SYSCAPE_EXECUTION_ENVIRONMENT_HPP

/// @file
/// @brief Allocation-free compile-target operating-system and environment facts.
/// @note Minimum compatibility profile: Freestanding Minimal.
/// @note Minimum language version: C++11; no hosted library is required.

#include <syscape/detail/config.hpp>

namespace syscape {

/// Identifies the operating system or runtime selected at compile time.
enum class operating_system {
    unknown,
    windows,
    linux_os,
    macos,
    android,
    ios,
    ipados,
    watchos,
    tvos,
    visionos,
    freebsd,
    openbsd,
    netbsd,
    dragonfly_bsd,
    illumos,
    solaris,
    aix,
    hpux,
    haiku,
    serenityos,
    redox,
    hurd,
    minix,
    qnx,
    vxworks,
    rtems,
    zephyr,
    nuttx,
    wasi,
    emscripten,
    openharmony,
    fuchsia,
    freertos,
    threadx,
    embos,
    ucos,
    integrity
};

/// Describes the broad execution restrictions of the compile target.
enum class execution_environment {
    unknown,
    hosted,
    sandboxed,
    compatibility,
    rtos,
    bare_metal
};

/// Returns the operating system or runtime selected for this translation unit.
constexpr operating_system target_operating_system() noexcept {
#if defined(SYSCAPE_FORCE_GENERIC_BACKEND) || defined(SYSCAPE_FORCE_UNKNOWN_TARGET)
    return operating_system::unknown;
#elif defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN) ||                        \
    defined(SYSCAPE_TARGET_EMSCRIPTEN)
    return operating_system::emscripten;
#elif defined(__wasi__) || defined(WASI) || defined(SYSCAPE_TARGET_WASI)
    return operating_system::wasi;
#elif defined(__ANDROID__)
    return operating_system::android;
#elif defined(__OHOS__) || defined(__OpenHarmony__)
    return operating_system::openharmony;
#elif defined(__Fuchsia__) || defined(FUCHSIA) ||                              \
    defined(SYSCAPE_TARGET_FUCHSIA)
    return operating_system::fuchsia;
#elif defined(__CYGWIN__) || defined(_WIN32)
    return operating_system::windows;
#elif defined(__APPLE__) && defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
    return operating_system::visionos;
#elif defined(__APPLE__) && defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__)
    return operating_system::watchos;
#elif defined(__APPLE__) && defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__)
    return operating_system::tvos;
#elif defined(__APPLE__) && defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
    return operating_system::ios;
#elif defined(__APPLE__) && defined(__MACH__)
    return operating_system::macos;
#elif defined(__FreeBSD__)
    return operating_system::freebsd;
#elif defined(__OpenBSD__)
    return operating_system::openbsd;
#elif defined(__NetBSD__)
    return operating_system::netbsd;
#elif defined(__DragonFly__)
    return operating_system::dragonfly_bsd;
#elif defined(__illumos__)
    return operating_system::illumos;
#elif defined(__sun) && defined(__SVR4)
    return operating_system::solaris;
#elif defined(SYSCAPE_TARGET_AIX)
    return operating_system::aix;
#elif defined(SYSCAPE_TARGET_HPUX) || defined(__hpux) || defined(__hpux__)
    return operating_system::hpux;
#elif defined(__HAIKU__)
    return operating_system::haiku;
#elif defined(__serenity__) || defined(SERENITY) ||                            \
    defined(SYSCAPE_TARGET_SERENITY)
    return operating_system::serenityos;
#elif defined(__redox__) || defined(REDOX) || defined(SYSCAPE_TARGET_REDOX)
    return operating_system::redox;
#elif defined(__GNU__)
    return operating_system::hurd;
#elif defined(__minix) || defined(__minix__) || defined(MINIX) ||              \
    defined(SYSCAPE_TARGET_MINIX)
    return operating_system::minix;
#elif defined(__QNXNTO__) || defined(__QNX__) || defined(QNX) ||               \
    defined(SYSCAPE_TARGET_QNX)
    return operating_system::qnx;
#elif defined(__VXWORKS__) || defined(_WRS_KERNEL) || defined(VXWORKS) ||      \
    defined(SYSCAPE_TARGET_VXWORKS)
    return operating_system::vxworks;
#elif defined(__rtems__) || defined(RTEMS) || defined(SYSCAPE_TARGET_RTEMS)
    return operating_system::rtems;
#elif defined(__ZEPHYR__) || defined(ZEPHYR) || defined(SYSCAPE_TARGET_ZEPHYR)
    return operating_system::zephyr;
#elif defined(__NuttX__) || defined(NUTTX) || defined(SYSCAPE_TARGET_NUTTX)
    return operating_system::nuttx;
#elif defined(FREERTOS) || defined(__FREERTOS__) ||                            \
    defined(SYSCAPE_TARGET_FREERTOS)
    return operating_system::freertos;
#elif defined(THREADX) || defined(__THREADX__) || defined(TX_API_H) ||         \
    defined(SYSCAPE_TARGET_THREADX)
    return operating_system::threadx;
#elif defined(EMBOS) || defined(__EMBOS__) || defined(RTOS_H) ||               \
    defined(SYSCAPE_TARGET_EMBOS)
    return operating_system::embos;
#elif defined(UCOS) || defined(__UCOS__) || defined(UCOS_II) ||                \
    defined(__UCOS_II__) || defined(UCOS_III) || defined(__UCOS_III__) ||      \
    defined(OS_uCOS_II) || defined(OS_uCOS_III) ||                             \
    defined(SYSCAPE_TARGET_UCOS)
    return operating_system::ucos;
#elif defined(__INTEGRITY) || defined(INTEGRITY) ||                            \
    defined(SYSCAPE_TARGET_INTEGRITY)
    return operating_system::integrity;
#elif defined(__linux__)
    return operating_system::linux_os;
#else
    return operating_system::unknown;
#endif
}

/// Returns the broad execution environment selected for this translation unit.
constexpr execution_environment target_execution_environment() noexcept {
#if defined(SYSCAPE_FORCE_UNKNOWN_TARGET)
    return execution_environment::unknown;
#elif defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN) ||                        \
    defined(SYSCAPE_TARGET_EMSCRIPTEN) || defined(__wasi__) ||                 \
    defined(WASI) || defined(SYSCAPE_TARGET_WASI) || defined(__ANDROID__) ||   \
    defined(__OHOS__) || defined(__OpenHarmony__) || defined(__Fuchsia__) ||   \
    defined(FUCHSIA) || defined(SYSCAPE_TARGET_FUCHSIA) ||                     \
    (defined(__APPLE__) &&                                                     \
     (defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) ||               \
      defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) ||                \
      defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) ||                   \
      defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)))
    return execution_environment::sandboxed;
#elif defined(__CYGWIN__) || defined(__MINGW32__) || defined(__MINGW64__)
    return execution_environment::compatibility;
#elif defined(__QNXNTO__) || defined(__QNX__) || defined(QNX) ||               \
    defined(SYSCAPE_TARGET_QNX) || defined(__VXWORKS__) ||                     \
    defined(_WRS_KERNEL) || defined(VXWORKS) ||                                \
    defined(SYSCAPE_TARGET_VXWORKS) || defined(__rtems__) || defined(RTEMS) || \
    defined(SYSCAPE_TARGET_RTEMS) || defined(__ZEPHYR__) || defined(ZEPHYR) || \
    defined(SYSCAPE_TARGET_ZEPHYR) || defined(__NuttX__) || defined(NUTTX) ||  \
    defined(SYSCAPE_TARGET_NUTTX) || defined(FREERTOS) ||                      \
    defined(__FREERTOS__) || defined(SYSCAPE_TARGET_FREERTOS) ||               \
    defined(THREADX) || defined(__THREADX__) || defined(TX_API_H) ||           \
    defined(SYSCAPE_TARGET_THREADX) || defined(EMBOS) || defined(__EMBOS__) || \
    defined(RTOS_H) || defined(SYSCAPE_TARGET_EMBOS) || defined(UCOS) ||       \
    defined(__UCOS__) || defined(UCOS_II) || defined(__UCOS_II__) ||           \
    defined(UCOS_III) || defined(__UCOS_III__) || defined(OS_uCOS_II) ||       \
    defined(OS_uCOS_III) || defined(SYSCAPE_TARGET_UCOS) ||                    \
    defined(__INTEGRITY) || defined(INTEGRITY) ||                              \
    defined(SYSCAPE_TARGET_INTEGRITY)
    return execution_environment::rtos;
#elif defined(__STDC_HOSTED__) && (__STDC_HOSTED__ == 0)
    return execution_environment::bare_metal;
#elif defined(__STDC_HOSTED__) && (__STDC_HOSTED__ == 1)
    return execution_environment::hosted;
#else
    return execution_environment::unknown;
#endif
}

/// Returns a stable English name for an operating-system value.
SYSCAPE_DETAIL_CONSTEXPR14 const char* operating_system_name(
    operating_system value) noexcept {
    switch (value) {
    case operating_system::windows: return "windows";
    case operating_system::linux_os: return "linux";
    case operating_system::macos: return "macos";
    case operating_system::android: return "android";
    case operating_system::ios: return "ios";
    case operating_system::ipados: return "ipados";
    case operating_system::watchos: return "watchos";
    case operating_system::tvos: return "tvos";
    case operating_system::visionos: return "visionos";
    case operating_system::freebsd: return "freebsd";
    case operating_system::openbsd: return "openbsd";
    case operating_system::netbsd: return "netbsd";
    case operating_system::dragonfly_bsd: return "dragonfly-bsd";
    case operating_system::illumos: return "illumos";
    case operating_system::solaris: return "solaris";
    case operating_system::aix: return "aix";
    case operating_system::hpux: return "hp-ux";
    case operating_system::haiku: return "haiku";
    case operating_system::serenityos: return "serenityos";
    case operating_system::redox: return "redox";
    case operating_system::hurd: return "hurd";
    case operating_system::minix:
        return "minix";
    case operating_system::qnx: return "qnx";
    case operating_system::vxworks: return "vxworks";
    case operating_system::rtems: return "rtems";
    case operating_system::zephyr: return "zephyr";
    case operating_system::nuttx: return "nuttx";
    case operating_system::openharmony:
        return "openharmony";
    case operating_system::wasi: return "wasi";
    case operating_system::emscripten: return "emscripten";
    case operating_system::fuchsia:
        return "fuchsia";
    case operating_system::freertos:
        return "freertos";
    case operating_system::threadx:
        return "threadx";
    case operating_system::embos:
        return "embos";
    case operating_system::ucos:
        return "ucos";
    case operating_system::integrity:
        return "integrity";
    case operating_system::unknown: return "unknown";
    }
    return "unknown";
}

/// Returns a stable English name for an execution-environment value.
SYSCAPE_DETAIL_CONSTEXPR14 const char* execution_environment_name(
    execution_environment value) noexcept {
    switch (value) {
    case execution_environment::hosted: return "hosted";
    case execution_environment::sandboxed: return "sandboxed";
    case execution_environment::compatibility: return "compatibility";
    case execution_environment::rtos: return "rtos";
    case execution_environment::bare_metal: return "bare-metal";
    case execution_environment::unknown: return "unknown";
    }
    return "unknown";
}

} // namespace syscape

#endif
