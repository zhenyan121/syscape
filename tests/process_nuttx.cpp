#include <iostream>

#include <syscape/process.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_process_queries() {
    const auto pid = syscape::process::process_id();
    expect(pid.has_value(), "process id query must succeed");

    const auto ppid = syscape::process::parent_process_id();
    expect(ppid.has_value(), "parent process id query must succeed");

    const auto threads = syscape::process::thread_count();
    expect(!threads && threads.error() == syscape::errc::not_supported,
           "thread count query must report not_supported on NuttX");

    const auto working_directory = syscape::process::working_directory();
    expect(working_directory && !working_directory->empty() &&
               working_directory->front() == '/',
           "working directory must be an absolute path");

    const auto exe = syscape::process::executable_path();
    expect(!exe && exe.error() == syscape::errc::not_supported,
           "executable path must report not_supported on NuttX");

    const auto cmdline = syscape::process::command_line();
    expect(cmdline.error() == syscape::errc::not_supported,
           "command line must report not_supported on NuttX");

    const auto cpu_time = syscape::process::cpu_time();
    expect(!cpu_time && cpu_time.error() == syscape::errc::not_supported,
           "CPU times must report not_supported on NuttX");

    const auto mem = syscape::process::memory_usage();
    expect(mem.error() == syscape::errc::not_supported,
           "memory usage must report not_supported on NuttX");

    const auto priority = syscape::process::priority();
    expect(priority && *priority >= 0,
           "process priority query must return a NuttX scheduling priority");

    const auto affinity = syscape::process::cpu_affinity();
    expect(affinity && !affinity->empty(),
           "CPU affinity must contain at least one processor");

    const auto limits = syscape::process::resource_limit(
        syscape::process::resource_kind::open_files);
    expect(limits.has_value(), "open-file resource limits must be available");

    const auto core = syscape::process::resource_limit(
        syscape::process::resource_kind::core_file_size);
    expect(!core && core.error() == syscape::errc::not_supported,
           "stubbed NuttX resource limits must remain unsupported");
}

} // namespace

int main() {
    test_process_queries();
    return failures == 0 ? 0 : 1;
}
