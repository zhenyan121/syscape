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
    expect(!loads && loads.error() == syscape::errc::not_supported,
           "load average query must report not_supported on Haiku r1beta5");
    const auto procs = syscape::resource::process_count();
    expect((procs && *procs > 0) ||
               procs.error() == syscape::errc::not_supported,
           "process count must be positive or not_supported");
    const auto threads = syscape::resource::thread_count();
    expect((threads && *threads > 0) ||
               threads.error() == syscape::errc::not_supported,
           "thread count must be positive or not_supported");
    const auto fd_lim = syscape::resource::file_descriptor_limit();
    expect((fd_lim && *fd_lim > 0) ||
               fd_lim.error() == syscape::errc::not_supported,
           "fd limit must be positive or not_supported");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
