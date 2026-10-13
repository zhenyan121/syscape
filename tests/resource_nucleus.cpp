#include <iostream>

#include "nucleus.h"

#include <syscape/resource.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void test_resource_queries() {
#if defined(SYSCAPE_NUCLEUS_THREAD_COUNT)
    const auto threads = syscape::resource::thread_count();
    expect(threads.has_value() && *threads == SYSCAPE_NUCLEUS_THREAD_COUNT,
           "thread count must report SYSCAPE_NUCLEUS_THREAD_COUNT");
#elif defined(NUCLEUS_THREAD_COUNT)
    const auto threads = syscape::resource::thread_count();
    expect(threads.has_value() && *threads == NUCLEUS_THREAD_COUNT,
           "thread count must report NUCLEUS_THREAD_COUNT");
#else
    const auto threads = syscape::resource::thread_count();
    expect(threads.has_value() && *threads == 4ULL,
           "thread count must report NU_Established_Tasks() on Nucleus RTOS");

    syscape_test_set_nucleus_established_tasks(8U);
    const auto threads8 = syscape::resource::thread_count();
    expect(threads8.has_value() && *threads8 == 8ULL,
           "thread count must reflect updated established task count");

    syscape_test_set_nucleus_established_tasks(0U);
    const auto threads0 = syscape::resource::thread_count();
    expect(!threads0 && threads0.error() == syscape::errc::not_supported,
           "thread count must report not_supported when task count is 0");

    syscape_test_reset_nucleus_mock();
#endif

    const auto load = syscape::resource::load_average();
    expect(!load && load.error() == syscape::errc::not_supported,
           "load average must report not_supported on Nucleus RTOS");

    const auto entities = syscape::resource::scheduler_entities();
    expect(!entities && entities.error() == syscape::errc::not_supported,
           "scheduler entities must report not_supported on Nucleus RTOS");

    const auto proc = syscape::resource::process_count();
    expect(!proc && proc.error() == syscape::errc::not_supported,
           "process count must report not_supported on Nucleus RTOS");

    const auto files = syscape::resource::open_file_count();
    expect(!files && files.error() == syscape::errc::not_supported,
           "open file count must report not_supported on Nucleus RTOS");

    const auto handles = syscape::resource::open_handle_count();
    expect(!handles && handles.error() == syscape::errc::not_supported,
           "open handle count must report not_supported on Nucleus RTOS");

    const auto limit = syscape::resource::file_descriptor_limit();
    expect(!limit && limit.error() == syscape::errc::not_supported,
           "file descriptor limit must report not_supported on Nucleus RTOS");
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
