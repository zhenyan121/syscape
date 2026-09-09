#include <iostream>

#include <syscape/environment.hpp>
#include <syscape/locale.hpp>
#include <syscape/process_list.hpp>
#include <syscape/user.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_disabled_capabilities() {
    const auto environment = syscape::environment::environment_variables();
    expect(!environment && environment.error() == syscape::errc::not_supported,
           "disabled environment storage must report not_supported");

    const auto variable = syscape::environment::get("PATH");
    expect(!variable && variable.error() == syscape::errc::not_supported,
           "disabled environment lookup must report not_supported");

    const auto locale = syscape::locale::current_locale();
    expect(!locale && locale.error() == syscape::errc::not_supported,
           "disabled locale support must report not_supported");

    const auto offset = syscape::locale::utc_offset_seconds();
    expect(!offset && offset.error() == syscape::errc::not_supported,
           "disabled local-time support must report not_supported");

    const auto identity = syscape::user::real_user_id();
    expect(!identity && identity.error() == syscape::errc::not_supported,
           "synthetic credentials must not be exposed as user identity");

    const auto processes = syscape::process_list::processes();
    expect(!processes && processes.error() == syscape::errc::not_supported,
           "disabled procfs process entries must report not_supported");

    // PATH search without '/' must report not_supported when environment is
    // disabled
    const auto path_executable = syscape::environment::find_executable("sh");
    expect(!path_executable &&
               path_executable.error() == syscape::errc::not_supported,
           "disabled environment must report not_supported for PATH-based "
           "find_executable");

    // Explicit path with '/' must not require environment variables
    const auto explicit_missing =
        syscape::environment::find_executable("/nonexistent/syscape_binary");
    expect(!explicit_missing &&
               explicit_missing.error() == syscape::errc::not_found,
           "explicit nonexistent path must report not_found even when "
           "environment is disabled");

    const auto explicit_sh = syscape::environment::find_executable("/bin/sh");
    expect(explicit_sh.has_value(), "explicit existing path must succeed even "
                                    "when environment is disabled");
}

} // namespace

int main() {
    test_disabled_capabilities();
    return failures == 0 ? 0 : 1;
}
