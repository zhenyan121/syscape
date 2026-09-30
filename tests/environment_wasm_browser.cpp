#include <iostream>

#include <syscape/environment.hpp>
#include <syscape/execution_environment.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_environment_queries() {
    expect(syscape::target_operating_system() ==
               syscape::operating_system::browser_wasm,
           "target_operating_system must report browser_wasm");

    const auto cwd = syscape::environment::current_working_directory();
    expect(cwd.has_value() || cwd.error() == syscape::errc::not_supported,
           "current_working_directory must succeed or report not_supported");

    const auto vars = syscape::environment::environment_variables();
    expect(vars.has_value() || vars.error() == syscape::errc::not_supported,
           "environment_variables must succeed or report not_supported");

    const auto missing =
        syscape::environment::get("SYSCAPE_NONEXISTENT_VAR_WASM_BROWSER");
    expect(
        (!missing && missing.error() == syscape::errc::not_found) ||
            (!missing && missing.error() == syscape::errc::not_supported),
        "missing environment variable must report not_found or not_supported");

    const auto tmp = syscape::environment::temp_directory();
    expect(tmp.has_value() || tmp.error() == syscape::errc::not_supported ||
               tmp.error() == syscape::errc::permission_denied,
           "temp_directory must succeed or report not_supported / "
           "permission_denied");

    const auto exe = syscape::environment::find_executable("test_exe");
    expect(!exe && exe.error() == syscape::errc::not_supported,
           "find_executable must report not_supported");
}

} // namespace

int main() {
    test_environment_queries();
    return failures == 0 ? 0 : 1;
}
