#ifndef SYSCAPE_DETAIL_ENVIRONMENT_WASM_BROWSER_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_WASM_BROWSER_HPP

#if defined(__has_include)
#if __has_include(<unistd.h>) && !defined(_WIN32)
#define SYSCAPE_DETAIL_WASM_BROWSER_HAS_POSIX_ENV 1
#endif
#elif !defined(_WIN32)
#define SYSCAPE_DETAIL_WASM_BROWSER_HAS_POSIX_ENV 1
#endif

#if defined(SYSCAPE_DETAIL_WASM_BROWSER_HAS_POSIX_ENV)
#include <syscape/detail/environment/emscripten.hpp>
#else
#include <syscape/detail/environment/generic.hpp>
#endif

#endif
