#include <syscape/execution_environment.hpp>

#include <cassert>
#include <cstring>

static void test_names() {
    assert(std::strcmp(syscape::operating_system_name(
                           syscape::operating_system::browser_wasm),
                       "browser-wasm") == 0);
    assert(std::strcmp(syscape::operating_system_name(
                           syscape::operating_system::embedded_wasm),
                       "embedded-wasm") == 0);
}

#if defined(TEST_WASM_BROWSER)
static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::browser_wasm,
              "SYSCAPE_TARGET_WASM_BROWSER must select browser_wasm");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::sandboxed,
              "browser_wasm must select sandboxed execution environment");
#if defined(SYSCAPE_TARGET_EMSCRIPTEN)
#error                                                                         \
    "SYSCAPE_TARGET_EMSCRIPTEN must NOT be defined when Browser WASM is active"
#endif
#elif defined(TEST_WASM_EMBEDDED)
static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::embedded_wasm,
              "SYSCAPE_TARGET_WASM_EMBEDDED must select embedded_wasm");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::sandboxed,
              "embedded_wasm must select sandboxed execution environment");
#if defined(SYSCAPE_TARGET_WASI)
#error "SYSCAPE_TARGET_WASI must NOT be defined when Embedded WASM is active"
#endif
#elif defined(TEST_WASM_NO_INTERP_MACRO)
static_assert(syscape::target_operating_system() !=
                  syscape::operating_system::embedded_wasm,
              "WASM_ENABLE_INTERP=0 must not select embedded_wasm");
#elif defined(TEST_WASM_EMBEDDED_MCU_ESP)
static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::embedded_wasm,
              "embedded_wasm must take precedence over MCU target");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::sandboxed,
              "embedded_wasm must report sandboxed even when MCU is defined");
#endif

int main() {
    test_names();
    return 0;
}
