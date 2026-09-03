#include <iostream>

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
    expect(cwd && !cwd->empty(), "current working directory must be nonempty");

    const auto temp = syscape::environment::temp_directory();
    expect(temp.has_value() || temp.error() == syscape::errc::not_supported,
           "temporary directory must succeed or report not_supported");

    const auto cfg = syscape::environment::config_directory();
    expect(cfg.has_value() || cfg.error() == syscape::errc::not_supported,
           "config directory must succeed or report not_supported");

    const auto data = syscape::environment::data_directory();
    expect(data.has_value() || data.error() == syscape::errc::not_supported,
           "data directory must succeed or report not_supported");

    const auto cache = syscape::environment::cache_directory();
    expect(cache.has_value() || cache.error() == syscape::errc::not_supported,
           "cache directory must succeed or report not_supported");
}

} // namespace

int main() {
    test_environment_queries();
    return failures == 0 ? 0 : 1;
}
