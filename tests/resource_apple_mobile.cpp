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
    const auto load = syscape::resource::load_average();
    expect(load.has_value() || load.error() == syscape::errc::not_supported ||
               load.error() == syscape::errc::io_error ||
               load.error() == syscape::errc::permission_denied,
           "load average query must succeed or report expected error");

    const auto pcount = syscape::resource::process_count();
    expect(pcount.has_value() ||
               pcount.error() == syscape::errc::not_supported ||
               pcount.error() == syscape::errc::permission_denied,
           "process count query must succeed or report expected error");

    const auto fd_limit = syscape::resource::file_descriptor_limit();
    expect(fd_limit.has_value() ||
               fd_limit.error() == syscape::errc::not_supported ||
               fd_limit.error() == syscape::errc::permission_denied,
           "file descriptor limit query must succeed or report expected error");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
