#ifndef SYSCAPE_DETAIL_CONFIG_HPP
#define SYSCAPE_DETAIL_CONFIG_HPP

#if defined(_MSVC_LANG)
#define SYSCAPE_DETAIL_CPLUSPLUS _MSVC_LANG
#else
#define SYSCAPE_DETAIL_CPLUSPLUS __cplusplus
#endif

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "Syscape requires C++17 or later"
#endif

#endif
