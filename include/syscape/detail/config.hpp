#ifndef SYSCAPE_DETAIL_CONFIG_HPP
#define SYSCAPE_DETAIL_CONFIG_HPP

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
