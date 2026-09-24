#include <cassert>
#include <syscape/cpu.hpp>
#include <syscape/filesystem.hpp>
#include <syscape/memory.hpp>
#include <syscape/os.hpp>
#include <syscape/process.hpp>
#include <syscape/resource.hpp>

int main() {
    // OS overrides
    {
        const auto name = syscape::os::product_name();
        assert(name.has_value());
        assert(*name == "z/OS");

        const auto ver = syscape::os::product_version();
        assert(ver.has_value());
        assert(*ver == "V2R5");

        const auto kver = syscape::os::kernel_version();
        assert(kver.has_value());
        assert(*kver == "V2R5");

        const auto hname = syscape::os::host_name();
        assert(hname.has_value());
        assert(*hname == "MAINFRAME1");

        const auto up = syscape::os::uptime();
        assert(up.has_value());
        assert(up->count() == 10800000);

        const auto boot = syscape::os::boot_time();
        assert(boot.has_value());
    }

    // CPU overrides
    {
        const auto logical = syscape::cpu::online_logical_processor_count();
        assert(logical.has_value());
        assert(*logical == 8U);

        const auto physical = syscape::cpu::online_physical_core_count();
        assert(physical.has_value());
        assert(*physical == 8U);

        const auto packages = syscape::cpu::online_processor_package_count();
        assert(packages.has_value());
        assert(*packages == 1U);
    }

    // Memory overrides & load percent calculation
    {
        const auto page = syscape::memory::page_size_bytes();
        assert(page.has_value());
        assert(*page == 8192ULL);

        const auto total = syscape::memory::physical_memory_bytes();
        assert(total.has_value());
        assert(*total == 68719476736ULL);

        const auto avail = syscape::memory::available_memory_bytes();
        assert(avail.has_value());
        assert(*avail == 17179869184ULL);

        const auto load = syscape::memory::memory_load_percent();
        assert(load.has_value());
        // (68719476736 - 17179869184) / 68719476736 = 75%
        assert(*load == 75U);
    }

    // Process overrides
    {
        const auto pid = syscape::process::process_id();
        assert(pid.has_value());
        assert(*pid == 500U);

        const auto ppid = syscape::process::parent_process_id();
        assert(ppid.has_value());
        assert(*ppid == 1U);

        const auto threads = syscape::process::thread_count();
        assert(threads.has_value());
        assert(*threads == 32U);

        const auto pri = syscape::process::priority();
        assert(pri.has_value());
        assert(*pri == 0);
    }

    // Resource overrides
    {
        const auto pcount = syscape::resource::process_count();
        assert(pcount.has_value());
        assert(*pcount == 128ULL);

        const auto tcount = syscape::resource::thread_count();
        assert(tcount.has_value());
        assert(*tcount == 1024ULL);

        const auto fd_lim = syscape::resource::file_descriptor_limit();
        assert(fd_lim.has_value());
        assert(*fd_lim == 1024ULL);
    }

    return 0;
}
