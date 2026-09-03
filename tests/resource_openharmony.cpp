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
    expect(loads.has_value() ||
               loads.error() == syscape::errc::permission_denied,
           "load average query must succeed or report permission_denied");

    const auto max_files = syscape::resource::file_descriptor_limit();
    expect((max_files && *max_files > 0) ||
               max_files.error() == syscape::errc::permission_denied ||
               max_files.error() == syscape::errc::not_supported,
           "file descriptor limit must be positive, report "
           "permission_denied, or report not_supported");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
