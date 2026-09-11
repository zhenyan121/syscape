#include <iostream>
#include <string>

#include <syscape/environment.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_environment_queries() {
    const auto cwd = syscape::environment::current_working_directory();
    expect(!cwd && cwd.error() == syscape::errc::not_supported,
           "current working directory must report not_supported on uC/OS");

    const auto tmp = syscape::environment::temp_directory();
    expect(!tmp && tmp.error() == syscape::errc::not_supported,
           "temp directory must report not_supported on uC/OS");

    const auto home = syscape::environment::home_directory();
    expect(!home && home.error() == syscape::errc::not_supported,
           "home directory query must report not_supported on uC/OS");

    const auto config = syscape::environment::config_directory();
    expect(!config && config.error() == syscape::errc::not_supported,
           "config directory must report not_supported on uC/OS");

    const auto data = syscape::environment::data_directory();
    expect(!data && data.error() == syscape::errc::not_supported,
           "data directory must report not_supported on uC/OS");

    const auto cache = syscape::environment::cache_directory();
    expect(!cache && cache.error() == syscape::errc::not_supported,
           "cache directory must report not_supported on uC/OS");

    const auto vars = syscape::environment::environment_variables();
    expect(!vars && vars.error() == syscape::errc::not_supported,
           "environment variables query must report not_supported on uC/OS");
}

} // namespace

int main() {
    test_environment_queries();
    return failures == 0 ? 0 : 1;
}
