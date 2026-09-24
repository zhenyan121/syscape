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
        assert(*name == "OpenVMS");

        const auto ver = syscape::os::product_version();
        assert(ver.has_value());
        assert(*ver == "V8.4-2L1");

        const auto kver = syscape::os::kernel_version();
        assert(kver.has_value());
        assert(*kver == "V8.4-2L1");

        const auto hname = syscape::os::host_name();
        assert(hname.has_value());
        assert(*hname == "NODE01");

        const auto up = syscape::os::uptime();
        assert(up.has_value());
        assert(up->count() == 7200000);

        const auto boot = syscape::os::boot_time();
        assert(boot.has_value());
    }

    // CPU overrides
    {
        const auto logical = syscape::cpu::online_logical_processor_count();
        assert(logical.has_value());
        assert(*logical == 4U);

        const auto physical = syscape::cpu::online_physical_core_count();
        assert(physical.has_value());
        assert(*physical == 4U);
    }

    // Memory overrides & load percent calculation
    {
        const auto total = syscape::memory::physical_memory_bytes();
        assert(total.has_value());
        assert(*total == 34359738368ULL);

        const auto avail = syscape::memory::available_memory_bytes();
        assert(avail.has_value());
        assert(*avail == 8589934592ULL);

        const auto load = syscape::memory::memory_load_percent();
        assert(load.has_value());
        // (34359738368 - 8589934592) / 34359738368 = 75%
        assert(*load == 75U);
    }

    // Process overrides
    {
        const auto pid = syscape::process::process_id();
        assert(pid.has_value());
        assert(*pid == 2048U);

        const auto ppid = syscape::process::parent_process_id();
        assert(ppid.has_value());
        assert(*ppid == 1024U);

        const auto threads = syscape::process::thread_count();
        assert(threads.has_value());
        assert(*threads == 16U);

        const auto pri = syscape::process::priority();
        assert(pri.has_value());
        assert(*pri == 16);
    }

    // Filesystem ODS-2 override
    {
        const auto comp = syscape::filesystem::max_component_length(".");
        assert(comp.has_value());
        assert(comp->length == 39U);

        const auto path = syscape::filesystem::max_path_length(".");
        assert(path.has_value());
        assert(path->length == 255U);
    }

    // Resource overrides
    {
        const auto pcount = syscape::resource::process_count();
        assert(pcount.has_value());
        assert(*pcount == 64ULL);

        const auto tcount = syscape::resource::thread_count();
        assert(tcount.has_value());
        assert(*tcount == 256ULL);

        const auto fd_lim = syscape::resource::file_descriptor_limit();
        assert(fd_lim.has_value());
        assert(*fd_lim == 4096ULL);
    }

    return 0;
}
