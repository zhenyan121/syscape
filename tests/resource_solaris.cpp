#include <iostream>

#include <syscape/resource.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_resource_queries() {
    const auto loads = syscape::resource::load_average();
    expect(loads && loads->one_minute >= 0.0,
           "load average query must succeed with nonnegative values");

    const auto limit = syscape::resource::file_descriptor_limit();
    expect(limit.error() == syscape::errc::not_supported,
           "system-level file descriptor limit must report not_supported on "
           "Solaris");

    const auto open_files = syscape::resource::open_file_count();
    expect(open_files.error() == syscape::errc::not_supported,
           "system-level open file count must report not_supported on Solaris");

    const auto proc_count = syscape::resource::process_count();
    expect(proc_count && *proc_count > 0, "process count must be positive");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
