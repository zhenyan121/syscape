#include <iostream>
#include <string>

#include <syscape/cpu.hpp>
#include <syscape/memory.hpp>
#include <syscape/os.hpp>
#include <syscape/resource.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_zephyr_posix_os() {
    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "Zephyr",
           "product name must be 'Zephyr'");

    const auto kernel_name = syscape::os::kernel_name();
    expect(kernel_name.has_value() && !kernel_name->empty(),
           "kernel name must be populated via uname");

    const auto kernel_version = syscape::os::kernel_version();
    expect(kernel_version.has_value() && !kernel_version->empty(),
           "kernel version must be populated via uname");

    const auto version = syscape::os::product_version();
    expect(version.has_value() && !version->empty(),
           "product version must be populated via uname");
    expect(version.has_value() && kernel_version.has_value() &&
               *version == *kernel_version,
           "product and kernel versions must both use uname release");

    const auto build = syscape::os::build_identifier();
    expect(build.has_value() && !build->empty(),
           "build identifier must be populated via uname version");

    const auto host = syscape::os::host_name();
    expect(host.has_value() && !host->empty(),
           "host name must be populated via uname");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() >= 0,
           "uptime must succeed via clock_gettime CLOCK_MONOTONIC");

    const auto started = syscape::os::boot_time();
    expect(started.error() == syscape::errc::not_supported,
           "boot time must report not_supported even with POSIX timers");
}

void test_zephyr_posix_cpu() {
    const auto count = syscape::cpu::online_logical_processor_count();
    expect(count.has_value() && *count >= 1U,
           "logical processor count must succeed via "
           "sysconf(_SC_NPROCESSORS_ONLN)");
}

void test_zephyr_posix_memory() {
    const auto page_size = syscape::memory::page_size_bytes();
    expect(
        page_size.has_value() && *page_size > 0 &&
            (*page_size & (*page_size - 1)) == 0,
        "page size must succeed via sysconf(_SC_PAGESIZE) and be power of two");
}

void test_zephyr_posix_resource() {
    const auto fd_lim = syscape::resource::file_descriptor_limit();
    expect(fd_lim.has_value() && *fd_lim > 0,
           "file descriptor limit must succeed via sysconf(_SC_OPEN_MAX)");
}

} // namespace

int main() {
    test_zephyr_posix_os();
    test_zephyr_posix_cpu();
    test_zephyr_posix_memory();
    test_zephyr_posix_resource();
    return failures == 0 ? 0 : 1;
}
