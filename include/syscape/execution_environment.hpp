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
    qnx,
    vxworks,
    rtems,
    zephyr,
    nuttx,
    wasi,
    emscripten,
    openharmony
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
#elif defined(__EMSCRIPTEN__)
    return operating_system::emscripten;
#elif defined(__wasi__)
    return operating_system::wasi;
#elif defined(__ANDROID__)
    return operating_system::android;
#elif defined(__OHOS__) || defined(__OpenHarmony__)
    return operating_system::openharmony;
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
#elif defined(__hpux)
    return operating_system::hpux;
#elif defined(__HAIKU__)
    return operating_system::haiku;
#elif defined(__serenity__)
    return operating_system::serenityos;
#elif defined(__redox__)
    return operating_system::redox;
#elif defined(__GNU__)
    return operating_system::hurd;
#elif defined(__QNXNTO__)
    return operating_system::qnx;
#elif defined(__VXWORKS__) || defined(_WRS_KERNEL)
    return operating_system::vxworks;
#elif defined(__rtems__)
    return operating_system::rtems;
#elif defined(__ZEPHYR__)
    return operating_system::zephyr;
#elif defined(__NuttX__)
    return operating_system::nuttx;
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
#elif defined(__EMSCRIPTEN__) || defined(__wasi__) || defined(__ANDROID__) ||  \
    defined(__OHOS__) || defined(__OpenHarmony__) ||                           \
    (defined(__APPLE__) &&                                                     \
     (defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) ||               \
      defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) ||                \
      defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) ||                   \
      defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)))
    return execution_environment::sandboxed;
#elif defined(__CYGWIN__) || defined(__MINGW32__) || defined(__MINGW64__)
    return execution_environment::compatibility;
#elif defined(__QNXNTO__) || defined(__VXWORKS__) || defined(_WRS_KERNEL) || \
    defined(__rtems__) || defined(__ZEPHYR__) || defined(__NuttX__)
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
    case operating_system::qnx: return "qnx";
    case operating_system::vxworks: return "vxworks";
    case operating_system::rtems: return "rtems";
    case operating_system::zephyr: return "zephyr";
    case operating_system::nuttx: return "nuttx";
    case operating_system::openharmony:
        return "openharmony";
    case operating_system::wasi: return "wasi";
    case operating_system::emscripten: return "emscripten";
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
