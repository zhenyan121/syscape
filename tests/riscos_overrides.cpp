#include <cassert>
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
        assert(*name == "RISC OS");

        const auto ver = syscape::os::product_version();
        assert(ver.has_value());
        assert(*ver == "5.30");

        const auto kver = syscape::os::kernel_version();
        assert(kver.has_value());
        assert(*kver == "5.30");

        const auto up = syscape::os::uptime();
        assert(up.has_value());
        assert(up->count() == 7200000);
    }

    // Memory overrides & load percent calculation
    {
        const auto total = syscape::memory::physical_memory_bytes();
        assert(total.has_value());
        assert(*total == 67108864ULL);

        const auto avail = syscape::memory::available_memory_bytes();
        assert(avail.has_value());
        assert(*avail == 33554432ULL);

        const auto load = syscape::memory::memory_load_percent();
        assert(load.has_value());
        // (67108864 - 33554432) / 67108864 = 50%
        assert(*load == 50U);
    }

    // Process overrides
    {
        const auto pid = syscape::process::process_id();
        assert(pid.has_value());
        assert(*pid == 200U);

        const auto ppid = syscape::process::parent_process_id();
        assert(ppid.has_value());
        assert(*ppid == 1U);

        const auto threads = syscape::process::thread_count();
        assert(threads.has_value());
        assert(*threads == 1U);

        const auto pri = syscape::process::priority();
        assert(pri.has_value());
        assert(*pri == 0);
    }

    // Filesystem classic limit override
    {
        const auto max_comp =
            syscape::filesystem::max_component_length("ADFS::0.$");
        assert(max_comp.has_value());
        assert(max_comp->length == 10U);
    }

    // Resource overrides
    {
        const auto pcount = syscape::resource::process_count();
        assert(pcount.has_value());
        assert(*pcount == 5ULL);

        const auto tcount = syscape::resource::thread_count();
        assert(tcount.has_value());
        assert(*tcount == 5ULL);

        const auto fd_lim = syscape::resource::file_descriptor_limit();
        assert(fd_lim.has_value());
        assert(*fd_lim == 128ULL);
    }

    return 0;
}
