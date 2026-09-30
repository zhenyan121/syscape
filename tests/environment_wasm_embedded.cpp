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
               syscape::operating_system::embedded_wasm,
           "target_operating_system must report embedded_wasm");

    const auto cwd = syscape::environment::current_working_directory();
    expect(
        !cwd && cwd.error() == syscape::errc::not_supported,
        "current_working_directory must report not_supported on embedded WASM");

    const auto vars = syscape::environment::environment_variables();
    expect(!vars && vars.error() == syscape::errc::not_supported,
           "environment_variables must report not_supported on embedded WASM");

    const auto val = syscape::environment::get("PATH");
    expect(!val && val.error() == syscape::errc::not_supported,
           "get must report not_supported on embedded WASM");

    const auto has = syscape::environment::has("PATH");
    expect(!has && has.error() == syscape::errc::not_supported,
           "has must report not_supported on embedded WASM");

    const auto tmp = syscape::environment::temp_directory();
    expect(!tmp && tmp.error() == syscape::errc::not_supported,
           "temp_directory must report not_supported on embedded WASM");

    const auto home = syscape::environment::home_directory();
    expect(!home && home.error() == syscape::errc::not_supported,
           "home_directory must report not_supported on embedded WASM");

    const auto exe = syscape::environment::find_executable("test");
    expect(!exe && exe.error() == syscape::errc::not_supported,
           "find_executable must report not_supported on embedded WASM");
}

} // namespace

int main() {
    test_environment_queries();
    return failures == 0 ? 0 : 1;
}
