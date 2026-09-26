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
        assert(*name == "Sailfish OS");

        const auto ver = syscape::os::product_version();
        assert(!ver.has_value());
        assert(ver.error() == syscape::errc::not_supported);

        const auto kname = syscape::os::kernel_name();
        assert(kname.has_value());
        assert(*kname == "Linux");

        const auto kver = syscape::os::kernel_version();
        assert(!kver.has_value());
        assert(kver.error() == syscape::errc::not_supported);

        const auto hname = syscape::os::host_name();
        assert(!hname.has_value());
        assert(hname.error() == syscape::errc::not_supported);

        const auto up = syscape::os::uptime();
        assert(!up.has_value());
        assert(up.error() == syscape::errc::not_supported);

        const auto boot = syscape::os::boot_time();
        assert(!boot.has_value());
        assert(boot.error() == syscape::errc::not_supported);

        const auto bid = syscape::os::build_identifier();
        assert(!bid.has_value());
        assert(bid.error() == syscape::errc::not_supported);

        const auto boot_id = syscape::os::boot_identifier();
        assert(!boot_id.has_value());
        assert(boot_id.error() == syscape::errc::not_supported);
    }

    // 2. CPU backend checks
    {
        const auto count = syscape::cpu::online_logical_processor_count();
        assert(!count.has_value());
        assert(count.error() == syscape::errc::not_supported);

        const auto phys = syscape::cpu::online_physical_core_count();
        assert(!phys.has_value());
        assert(phys.error() == syscape::errc::not_supported);

        const auto pkgs = syscape::cpu::online_processor_package_count();
        assert(!pkgs.has_value());
        assert(pkgs.error() == syscape::errc::not_supported);

        const auto vendors = syscape::cpu::vendor_identifiers();
        assert(!vendors.has_value());
        assert(vendors.error() == syscape::errc::not_supported);

        const auto models = syscape::cpu::model_names();
        assert(!models.has_value());
        assert(models.error() == syscape::errc::not_supported);

        const auto min_freq = syscape::cpu::minimum_frequency_khz();
        assert(!min_freq.has_value());
        assert(min_freq.error() == syscape::errc::not_supported);

        const auto max_freq = syscape::cpu::maximum_frequency_khz();
        assert(!max_freq.has_value());
        assert(max_freq.error() == syscape::errc::not_supported);

        const auto cur_freqs = syscape::cpu::current_frequencies_khz();
        assert(!cur_freqs.has_value());
        assert(cur_freqs.error() == syscape::errc::not_supported);

        const auto caches = syscape::cpu::cache_descriptors();
        assert(!caches.has_value());
        assert(caches.error() == syscape::errc::not_supported);

        const auto isas = syscape::cpu::instruction_set_features();
        assert(!isas.has_value());
        assert(isas.error() == syscape::errc::not_supported);

        const auto usage = syscape::cpu::cumulative_processor_usage();
        assert(!usage.has_value());
        assert(usage.error() == syscape::errc::not_supported);
    }

    // 3. Memory backend checks
    {
        const auto page = syscape::memory::page_size_bytes();
        assert(page.has_value());
        assert(*page > 0U);
        assert((*page & (*page - 1U)) == 0U);

        const auto phys = syscape::memory::physical_memory_bytes();
        assert(!phys.has_value());
        assert(phys.error() == syscape::errc::not_supported);

        const auto avail = syscape::memory::available_memory_bytes();
        assert(!avail.has_value());
        assert(avail.error() == syscape::errc::not_supported);

        const auto load = syscape::memory::memory_load_percent();
        assert(!load.has_value());
        assert(load.error() == syscape::errc::not_supported);

        const auto swap = syscape::memory::swap_status();
        assert(!swap.has_value());
        assert(swap.error() == syscape::errc::not_supported);

        const auto commit = syscape::memory::commit_status();
        assert(!commit.has_value());
        assert(commit.error() == syscape::errc::not_supported);

        const auto hpage = syscape::memory::huge_page_size_bytes();
        assert(!hpage.has_value());
        assert(hpage.error() == syscape::errc::not_supported);

        const auto hpool = syscape::memory::huge_page_pool_status();
        assert(!hpool.has_value());
        assert(hpool.error() == syscape::errc::not_supported);

        const auto pressure = syscape::memory::memory_pressure();
        assert(!pressure.has_value());
        assert(pressure.error() == syscape::errc::not_supported);
    }

    // 4. Process backend checks
    {
        const auto pid = syscape::process::process_id();
        assert(!pid.has_value());
        assert(pid.error() == syscape::errc::not_supported);

        const auto ppid = syscape::process::parent_process_id();
        assert(!ppid.has_value());
        assert(ppid.error() == syscape::errc::not_supported);

        const auto exe = syscape::process::executable_path();
        assert(!exe.has_value());
        assert(exe.error() == syscape::errc::not_supported);

        const auto cmd = syscape::process::command_line();
        assert(!cmd.has_value());
        assert(cmd.error() == syscape::errc::not_supported);

        const auto cwd = syscape::process::working_directory();
        assert(!cwd.has_value());
        assert(cwd.error() == syscape::errc::not_supported);

        const auto times = syscape::process::cpu_time();
        assert(!times.has_value());
        assert(times.error() == syscape::errc::not_supported);

        const auto start = syscape::process::start_time();
        assert(!start.has_value());
        assert(start.error() == syscape::errc::not_supported);

        const auto mem = syscape::process::memory_usage();
        assert(!mem.has_value());
        assert(mem.error() == syscape::errc::not_supported);

        const auto thr = syscape::process::thread_count();
        assert(!thr.has_value());
        assert(thr.error() == syscape::errc::not_supported);

        const auto pri = syscape::process::priority();
        assert(!pri.has_value());
        assert(pri.error() == syscape::errc::not_supported);

        const auto aff = syscape::process::cpu_affinity();
        assert(!aff.has_value());
        assert(aff.error() == syscape::errc::not_supported);

        const auto lim = syscape::process::resource_limit(
            syscape::process::resource_kind::open_files);
        assert(!lim.has_value());
        assert(lim.error() == syscape::errc::not_supported);
    }

    // 5. Filesystem backend checks
    {
        const auto comp = syscape::filesystem::max_component_length(".");
        assert(comp.has_value());
        assert(comp->length > 0U || comp->indeterminate);

        const auto path = syscape::filesystem::max_path_length(".");
        assert(path.has_value());
        assert(path->length > 0U || path->indeterminate);

        const auto mounts = syscape::filesystem::mounts();
        assert(!mounts.has_value());
        assert(mounts.error() == syscape::errc::not_supported);

        const auto sp = syscape::filesystem::space(".");
        assert(!sp.has_value());
        assert(sp.error() == syscape::errc::not_supported);

        const auto vid = syscape::filesystem::volume_id(".");
        assert(!vid.has_value());
        assert(vid.error() == syscape::errc::not_supported);
    }

    // 6. Resource backend checks
    {
        const auto fd = syscape::resource::file_descriptor_limit();
        assert(!fd.has_value());
        assert(fd.error() == syscape::errc::not_supported);

        const auto load = syscape::resource::load_average();
        assert(!load.has_value());
        assert(load.error() == syscape::errc::not_supported);

        const auto pcount = syscape::resource::process_count();
        assert(!pcount.has_value());
        assert(pcount.error() == syscape::errc::not_supported);

        const auto tcount = syscape::resource::thread_count();
        assert(!tcount.has_value());
        assert(tcount.error() == syscape::errc::not_supported);

        const auto entities = syscape::resource::scheduler_entities();
        assert(!entities.has_value());
        assert(entities.error() == syscape::errc::not_supported);

        const auto open_f = syscape::resource::open_file_count();
        assert(!open_f.has_value());
        assert(open_f.error() == syscape::errc::not_supported);

        const auto open_h = syscape::resource::open_handle_count();
        assert(!open_h.has_value());
        assert(open_h.error() == syscape::errc::not_supported);
    }

    // 7. Generic fallback checks across remaining 23 modules
    {
        const auto audio_devs = syscape::audio::devices();
        assert(!audio_devs.has_value());
        assert(audio_devs.error() == syscape::errc::not_supported);

        const auto bt = syscape::bluetooth::adapters();
        assert(!bt.has_value());
        assert(bt.error() == syscape::errc::not_supported);

        const auto cams = syscape::camera::devices();
        assert(!cams.has_value());
        assert(cams.error() == syscape::errc::not_supported);

        const auto conns = syscape::connection::tcp_connections();
        assert(!conns.has_value());
        assert(conns.error() == syscape::errc::not_supported);

        const auto disps = syscape::display::displays();
        assert(!disps.has_value());
        assert(disps.error() == syscape::errc::not_supported);

        const auto env_var = syscape::environment::get("PATH");
        assert(!env_var.has_value());
        assert(env_var.error() == syscape::errc::not_supported);

        const auto gpu_devs = syscape::gpu::devices();
        assert(!gpu_devs.has_value());
        assert(gpu_devs.error() == syscape::errc::not_supported);

        const auto mfg = syscape::hardware::system_manufacturer();
        assert(!mfg.has_value());
        assert(mfg.error() == syscape::errc::not_supported);

        const auto in_devs = syscape::input::devices();
        assert(!in_devs.has_value());
        assert(in_devs.error() == syscape::errc::not_supported);

        const auto shm = syscape::ipc::shared_memory_segments();
        assert(!shm.has_value());
        assert(shm.error() == syscape::errc::not_supported);

        const auto loc = syscape::locale::current_locale();
        assert(!loc.has_value());
        assert(loc.error() == syscape::errc::not_supported);

        const auto ifaces = syscape::network::interfaces();
        assert(!ifaces.has_value());
        assert(ifaces.error() == syscape::errc::not_supported);

        const auto nodes = syscape::numa::nodes();
        assert(!nodes.has_value());
        assert(nodes.error() == syscape::errc::not_supported);

        const auto batts = syscape::power::batteries();
        assert(!batts.has_value());
        assert(batts.error() == syscape::errc::not_supported);

        const auto printers = syscape::printer::printers();
        assert(!printers.has_value());
        assert(printers.error() == syscape::errc::not_supported);

        const auto plist = syscape::process_list::processes();
        assert(!plist.has_value());
        assert(plist.error() == syscape::errc::not_supported);

        const auto sec = syscape::security::is_secure_boot_enabled();
        assert(!sec.has_value());
        assert(sec.error() == syscape::errc::not_supported);

        const auto sens = syscape::sensor::temperatures();
        assert(!sens.has_value());
        assert(sens.error() == syscape::errc::not_supported);

        const auto pkgs = syscape::software::installed_packages();
        assert(!pkgs.has_value());
        assert(pkgs.error() == syscape::errc::not_supported);

        const auto disks = syscape::storage::drives();
        assert(!disks.has_value());
        assert(disks.error() == syscape::errc::not_supported);

        const auto usr = syscape::user::user_name();
        assert(!usr.has_value());
        assert(usr.error() == syscape::errc::not_supported);

        const auto hv = syscape::virtualization::is_hypervisor_present();
        assert(!hv.has_value());
        assert(hv.error() == syscape::errc::not_supported);

        const auto wlan = syscape::wifi::adapters();
        assert(!wlan.has_value());
        assert(wlan.error() == syscape::errc::not_supported);
    }

    return 0;
}
