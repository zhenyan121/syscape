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
    const auto fd_limit = syscape::resource::file_descriptor_limit();
    expect(!fd_limit && fd_limit.error() == syscape::errc::not_supported,
           "file_descriptor_limit must report not_supported on WASI");

    const auto load = syscape::resource::load_average();
    expect(!load && load.error() == syscape::errc::not_supported,
           "load_average must report not_supported on WASI");

    const auto procs = syscape::resource::process_count();
    expect(!procs && procs.error() == syscape::errc::not_supported,
           "process_count must report not_supported on WASI");

    const auto threads = syscape::resource::thread_count();
    expect(!threads && threads.error() == syscape::errc::not_supported,
           "thread_count must report not_supported on WASI");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
