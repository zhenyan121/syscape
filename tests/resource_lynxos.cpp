#include <iostream>

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
    const auto load = syscape::resource::load_average();
    expect(!load && load.error() == syscape::errc::not_supported,
           "load_average must report not_supported on LynxOS");

    const auto entities = syscape::resource::scheduler_entities();
    expect(!entities && entities.error() == syscape::errc::not_supported,
           "scheduler_entities must report not_supported on LynxOS");

    const auto procs = syscape::resource::process_count();
#if defined(SYSCAPE_LYNXOS_PROCESS_COUNT)
    expect(procs.has_value() && *procs == SYSCAPE_LYNXOS_PROCESS_COUNT,
           "process_count must match SYSCAPE_LYNXOS_PROCESS_COUNT");
#elif defined(LYNXOS_PROCESS_COUNT)
    expect(procs.has_value() && *procs == LYNXOS_PROCESS_COUNT,
           "process_count must match LYNXOS_PROCESS_COUNT");
#else
    expect(!procs && procs.error() == syscape::errc::not_supported,
           "process_count must report not_supported on LynxOS by default");
#endif

    const auto threads = syscape::resource::thread_count();
#if defined(SYSCAPE_LYNXOS_THREAD_COUNT)
    expect(threads.has_value() && *threads == SYSCAPE_LYNXOS_THREAD_COUNT,
           "thread_count must match SYSCAPE_LYNXOS_THREAD_COUNT");
#elif defined(LYNXOS_THREAD_COUNT)
    expect(threads.has_value() && *threads == LYNXOS_THREAD_COUNT,
           "thread_count must match LYNXOS_THREAD_COUNT");
#elif defined(SYSCAPE_LYNXOS_TASK_COUNT)
    expect(threads.has_value() && *threads == SYSCAPE_LYNXOS_TASK_COUNT,
           "thread_count must match SYSCAPE_LYNXOS_TASK_COUNT");
#elif defined(LYNXOS_TASK_COUNT)
    expect(threads.has_value() && *threads == LYNXOS_TASK_COUNT,
           "thread_count must match LYNXOS_TASK_COUNT");
#else
    expect(!threads && threads.error() == syscape::errc::not_supported,
           "thread_count must report not_supported on LynxOS by default");
#endif

    const auto files = syscape::resource::open_file_count();
    expect(!files && files.error() == syscape::errc::not_supported,
           "open_file_count must report not_supported on LynxOS");

    const auto handles = syscape::resource::open_handle_count();
    expect(!handles && handles.error() == syscape::errc::not_supported,
           "open_handle_count must report not_supported on LynxOS");

    const auto fd_lim = syscape::resource::file_descriptor_limit();
    if (fd_lim.has_value()) {
        expect(*fd_lim > 0, "file_descriptor_limit must be positive");
    } else {
        expect(fd_lim.error() == syscape::errc::not_supported ||
                   fd_lim.error().category() == std::generic_category(),
               "file_descriptor_limit failure must report not_supported or "
               "native error");
    }
}

} // namespace

int main() {
    test_resource_queries();
    return failures == 0 ? 0 : 1;
}
