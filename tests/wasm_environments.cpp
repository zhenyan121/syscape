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
#elif defined(TEST_WASM_EMBEDDED)
static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::embedded_wasm,
              "SYSCAPE_TARGET_WASM_EMBEDDED must select embedded_wasm");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::sandboxed,
              "embedded_wasm must select sandboxed execution environment");
#endif

int main() {
    test_names();
    return 0;
}
