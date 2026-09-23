#include <cassert>
#include <syscape/audio.hpp>
#include <syscape/bluetooth.hpp>
#include <syscape/camera.hpp>
#include <syscape/connection.hpp>
#include <syscape/cpu.hpp>
#include <syscape/display.hpp>
#include <syscape/environment.hpp>
#include <syscape/filesystem.hpp>
#include <syscape/gpu.hpp>
#include <syscape/hardware.hpp>
#include <syscape/input.hpp>
#include <syscape/ipc.hpp>
#include <syscape/locale.hpp>
#include <syscape/memory.hpp>
#include <syscape/network.hpp>
#include <syscape/numa.hpp>
#include <syscape/os.hpp>
#include <syscape/power.hpp>
#include <syscape/printer.hpp>
#include <syscape/process.hpp>
#include <syscape/process_list.hpp>
#include <syscape/resource.hpp>
#include <syscape/security.hpp>
#include <syscape/sensor.hpp>
#include <syscape/software.hpp>
#include <syscape/storage.hpp>
#include <syscape/user.hpp>
#include <syscape/virtualization.hpp>
#include <syscape/wifi.hpp>

int main() {
    // 1. OS backend checks
    {
        const auto name = syscape::os::product_name();
        assert(name.has_value());
        assert(*name == "Cygwin");

        const auto kname = syscape::os::kernel_name();
        assert(kname.has_value());
        assert(!kname->empty());

        const auto hname = syscape::os::host_name();
        assert(hname.has_value());
        assert(!hname->empty());

        const auto up = syscape::os::uptime();
        assert(up.has_value());
        assert(up->count() > 0);

        const auto build = syscape::os::build_identifier();
        assert(!build.has_value());
        assert(build.error() == syscape::errc::not_supported);
    }

    // 2. CPU backend checks
    {
        const auto logical = syscape::cpu::online_logical_processor_count();
        assert(logical.has_value());
        assert(*logical >= 1U);

        const auto physical = syscape::cpu::online_physical_core_count();
        assert(!physical.has_value());
        assert(physical.error() == syscape::errc::not_supported);
    }

    // 3. Memory backend checks
    {
        const auto page = syscape::memory::page_size_bytes();
        assert(page.has_value());
        assert(*page > 0U);

        const auto total = syscape::memory::physical_memory_bytes();
        assert(total.has_value());
        assert(*total > 0U);

        const auto avail = syscape::memory::available_memory_bytes();
        assert(avail.has_value());
    }

    // 4. Process backend checks
    {
        const auto pid = syscape::process::process_id();
        assert(pid.has_value());

        const auto ppid = syscape::process::parent_process_id();
        assert(ppid.has_value());
    }

    // 5. Filesystem backend checks
    {
        const auto sp = syscape::filesystem::space(".");
        assert(sp.has_value());
        assert(sp->capacity_bytes > 0U);

        const auto max_comp = syscape::filesystem::max_component_length(".");
        assert(max_comp.has_value());

        const auto max_path = syscape::filesystem::max_path_length(".");
        assert(max_path.has_value());

        const auto mounts = syscape::filesystem::mounts();
        assert(!mounts.has_value());
        assert(mounts.error() == syscape::errc::not_supported);
    }

    // 6. Network backend checks
    {
        const auto ifaces = syscape::network::interfaces();
        assert(ifaces.has_value());

        const auto rts = syscape::network::routes();
        assert(!rts.has_value());
        assert(rts.error() == syscape::errc::not_supported);
    }

    // 7. Resource backend checks
    {
        const auto fd_lim = syscape::resource::file_descriptor_limit();
        assert(fd_lim.has_value());
        assert(*fd_lim > 0U);

        const auto load = syscape::resource::load_average();
        assert(load.has_value());

        const auto pcount = syscape::resource::process_count();
        assert(!pcount.has_value());
        assert(pcount.error() == syscape::errc::not_supported);
    }

    // 8. Locale backend checks
    {
        const auto loc = syscape::locale::current_locale();
        assert(loc.has_value());
    }

    // 9. Delegating headers return not_supported
    {
        const auto gpu_devs = syscape::gpu::devices();
        assert(!gpu_devs.has_value());
        assert(gpu_devs.error() == syscape::errc::not_supported);

        const auto audio_devs = syscape::audio::devices();
        assert(!audio_devs.has_value());
        assert(audio_devs.error() == syscape::errc::not_supported);

        const auto bt = syscape::bluetooth::adapters();
        assert(!bt.has_value());
        assert(bt.error() == syscape::errc::not_supported);

        const auto cams = syscape::camera::devices();
        assert(!cams.has_value());
        assert(cams.error() == syscape::errc::not_supported);

        const auto in_devs = syscape::input::devices();
        assert(!in_devs.has_value());
        assert(in_devs.error() == syscape::errc::not_supported);

        const auto batts = syscape::power::batteries();
        assert(!batts.has_value());
        assert(batts.error() == syscape::errc::not_supported);

        const auto printers = syscape::printer::printers();
        assert(!printers.has_value());
        assert(printers.error() == syscape::errc::not_supported);

        const auto sec = syscape::security::is_secure_boot_enabled();
        assert(!sec.has_value());
        assert(sec.error() == syscape::errc::not_supported);

        const auto sens = syscape::sensor::temperatures();
        assert(!sens.has_value());
        assert(sens.error() == syscape::errc::not_supported);

        const auto wlan = syscape::wifi::adapters();
        assert(!wlan.has_value());
        assert(wlan.error() == syscape::errc::not_supported);

        const auto nodes = syscape::numa::nodes();
        assert(!nodes.has_value());
        assert(nodes.error() == syscape::errc::not_supported);

        const auto plist = syscape::process_list::processes();
        assert(!plist.has_value());
        assert(plist.error() == syscape::errc::not_supported);

        const auto pkgs = syscape::software::installed_packages();
        assert(!pkgs.has_value());
        assert(pkgs.error() == syscape::errc::not_supported);

        const auto conns = syscape::connection::tcp_connections();
        assert(!conns.has_value());
        assert(conns.error() == syscape::errc::not_supported);

        const auto disks = syscape::storage::drives();
        assert(!disks.has_value());
        assert(disks.error() == syscape::errc::not_supported);

        const auto wine = syscape::virtualization::is_wine();
        assert(wine.has_value());
        assert(*wine == false);
    }

    return 0;
}
