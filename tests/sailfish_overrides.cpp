#include <cassert>
#include <chrono>
#include <syscape/cpu.hpp>
#include <syscape/filesystem.hpp>
#include <syscape/memory.hpp>
#include <syscape/os.hpp>
#include <syscape/process.hpp>
#include <syscape/resource.hpp>

int main() {
    // OS overrides
    {
        const auto ver = syscape::os::product_version();
        assert(ver.has_value());
        assert(*ver == "3.0.0");

        const auto kname = syscape::os::kernel_name();
        assert(kname.has_value());
        assert(*kname == "Linux-custom");

        const auto kver = syscape::os::kernel_version();
        assert(kver.has_value());
        assert(*kver == "5.4.0");

        const auto hname = syscape::os::host_name();
        assert(hname.has_value());
        assert(*hname == "my-sailfish-device");

        const auto up = syscape::os::uptime();
        assert(up.has_value());
        assert(up->count() == 123456);

        const auto boot = syscape::os::boot_time();
        assert(boot.has_value());
    }

    // CPU overrides
    {
        const auto count = syscape::cpu::online_logical_processor_count();
        assert(count.has_value());
        assert(*count == 8U);

        const auto phys = syscape::cpu::online_physical_core_count();
        assert(phys.has_value());
        assert(*phys == 4U);

        const auto pkgs = syscape::cpu::online_processor_package_count();
        assert(pkgs.has_value());
        assert(*pkgs == 1U);
    }

    // Memory overrides
    {
        const auto page = syscape::memory::page_size_bytes();
        assert(page.has_value());
        assert(*page == 4096U);

        const auto phys = syscape::memory::physical_memory_bytes();
        assert(phys.has_value());
        assert(*phys == 4294967296ULL);

        const auto avail = syscape::memory::available_memory_bytes();
        assert(avail.has_value());
        assert(*avail == 2147483648ULL);

        const auto load = syscape::memory::memory_load_percent();
        assert(load.has_value());
        assert(*load == 50U);
    }

    // Process overrides
    {
        const auto pid = syscape::process::process_id();
        assert(pid.has_value());
        assert(*pid == 100U);

        const auto ppid = syscape::process::parent_process_id();
        assert(ppid.has_value());
        assert(*ppid == 1U);

        const auto threads = syscape::process::thread_count();
        assert(threads.has_value());
        assert(*threads == 16U);

        const auto prio = syscape::process::priority();
        assert(prio.has_value());
        assert(*prio == 5);
    }

    // Filesystem overrides
    {
        const auto path_max = syscape::filesystem::max_path_length(".");
        assert(path_max.has_value());
        assert(path_max->length == 2048U);
    }

    // Resource overrides
    {
        const auto procs = syscape::resource::process_count();
        assert(procs.has_value());
        assert(*procs == 42U);

        const auto threads = syscape::resource::thread_count();
        assert(threads.has_value());
        assert(*threads == 128U);

        const auto fd_lim = syscape::resource::file_descriptor_limit();
        assert(fd_lim.has_value());
        assert(*fd_lim == 1024U);
    }

    return 0;
}
