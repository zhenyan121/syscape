#include <cassert>
#include <syscape/cpu.hpp>
#include <syscape/filesystem.hpp>
#include <syscape/memory.hpp>
#include <syscape/process.hpp>
#include <syscape/resource.hpp>

int main() {
    // 1. CPU override range & zero validation
    {
        const auto cpu_count = syscape::cpu::online_logical_processor_count();
        assert(!cpu_count.has_value());
        assert(cpu_count.error() == syscape::errc::malformed_data);

        const auto phys_count = syscape::cpu::online_physical_core_count();
        assert(!phys_count.has_value());
        assert(phys_count.error() == syscape::errc::value_too_large);

        const auto pkg_count = syscape::cpu::online_processor_package_count();
        assert(!pkg_count.has_value());
        assert(pkg_count.error() == syscape::errc::malformed_data);
    }

    // 2. Memory override range & zero validation
    {
        const auto phys = syscape::memory::physical_memory_bytes();
        assert(!phys.has_value());
        assert(phys.error() == syscape::errc::malformed_data);

        const auto avail = syscape::memory::available_memory_bytes();
        assert(!avail.has_value());
        assert(avail.error() == syscape::errc::malformed_data);

        const auto page = syscape::memory::page_size_bytes();
        assert(!page.has_value());
        assert(page.error() == syscape::errc::malformed_data);
    }

    // 3. Process override range & zero validation
    {
        const auto pid = syscape::process::process_id();
        assert(!pid.has_value());
        assert(pid.error() == syscape::errc::malformed_data);

        const auto ppid = syscape::process::parent_process_id();
        assert(!ppid.has_value());
        assert(ppid.error() == syscape::errc::malformed_data);

        const auto thr = syscape::process::thread_count();
        assert(!thr.has_value());
        assert(thr.error() == syscape::errc::value_too_large);
    }

    // 4. Filesystem override range & zero validation
    {
        const auto comp = syscape::filesystem::max_component_length(".");
        assert(!comp.has_value());
        assert(comp.error() == syscape::errc::malformed_data);

        const auto path = syscape::filesystem::max_path_length(".");
        assert(!path.has_value());
        assert(path.error() == syscape::errc::malformed_data);
    }

    // 5. Resource override range & zero validation
    {
        const auto procs = syscape::resource::process_count();
        assert(!procs.has_value());
        assert(procs.error() == syscape::errc::malformed_data);

        const auto thr = syscape::resource::thread_count();
        assert(!thr.has_value());
        assert(thr.error() == syscape::errc::malformed_data);

        const auto fd = syscape::resource::file_descriptor_limit();
        assert(!fd.has_value());
        assert(fd.error() == syscape::errc::malformed_data);
    }

    return 0;
}
