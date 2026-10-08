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
#if defined(TI_RTOS_THREAD_COUNT)
    const auto threads = syscape::resource::thread_count();
    expect(threads.has_value() && *threads == TI_RTOS_THREAD_COUNT,
           "thread count must report TI_RTOS_THREAD_COUNT");
#elif defined(SYSCAPE_TI_RTOS_THREAD_COUNT)
    const auto threads = syscape::resource::thread_count();
    expect(threads.has_value() && *threads == SYSCAPE_TI_RTOS_THREAD_COUNT,
           "thread count must report SYSCAPE_TI_RTOS_THREAD_COUNT");
#else
    const auto threads = syscape::resource::thread_count();
    expect(
        !threads && threads.error() == syscape::errc::not_supported,
        "thread count must report not_supported on TI-RTOS when unconfigured");
#endif

    const auto load = syscape::resource::load_average();
    expect(!load && load.error() == syscape::errc::not_supported,
           "load average must report not_supported on TI-RTOS");

    const auto entities = syscape::resource::scheduler_entities();
    expect(!entities && entities.error() == syscape::errc::not_supported,
           "scheduler entities must report not_supported on TI-RTOS");

    const auto proc = syscape::resource::process_count();
    expect(!proc && proc.error() == syscape::errc::not_supported,
           "process count must report not_supported on TI-RTOS");

    const auto files = syscape::resource::open_file_count();
    expect(!files && files.error() == syscape::errc::not_supported,
           "open file count must report not_supported on TI-RTOS");

    const auto handles = syscape::resource::open_handle_count();
    expect(!handles && handles.error() == syscape::errc::not_supported,
           "open handle count must report not_supported on TI-RTOS");

    const auto limit = syscape::resource::file_descriptor_limit();
    expect(!limit && limit.error() == syscape::errc::not_supported,
           "file descriptor limit must report not_supported on TI-RTOS");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
