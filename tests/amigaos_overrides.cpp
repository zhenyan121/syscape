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
        assert(*name == "AmigaOS");

        const auto ver = syscape::os::product_version();
        assert(ver.has_value());
        assert(*ver == "3.9");

        const auto up = syscape::os::uptime();
        assert(up.has_value());
        assert(up->count() == 3600000);
    }

    // Memory overrides & load percent calculation
    {
        const auto total = syscape::memory::physical_memory_bytes();
        assert(total.has_value());
        assert(*total == 16777216ULL);

        const auto avail = syscape::memory::available_memory_bytes();
        assert(avail.has_value());
        assert(*avail == 4194304ULL);

        const auto load = syscape::memory::memory_load_percent();
        assert(load.has_value());
        // (16777216 - 4194304) / 16777216 = 75%
        assert(*load == 75U);
    }

    // Process overrides
    {
        const auto pri = syscape::process::priority();
        assert(pri.has_value());
        assert(*pri == 10);
    }

    // Resource overrides
    {
        const auto fd_lim = syscape::resource::file_descriptor_limit();
        assert(fd_lim.has_value());
        assert(*fd_lim == 50ULL);
    }

    return 0;
}
