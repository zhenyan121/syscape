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

#if defined(_AIX) || defined(__TOS_AIX__)
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
