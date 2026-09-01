#include <system_error>
#include <vector>

#include <syscape/architecture.hpp>
#include <syscape/audio.hpp>
#include <syscape/bluetooth.hpp>
#include <syscape/camera.hpp>
#include <syscape/connection.hpp>
#include <syscape/cpu.hpp>
#include <syscape/display.hpp>
#include <syscape/environment.hpp>
#include <syscape/error.hpp>
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
#include <syscape/result.hpp>
#include <syscape/security.hpp>
#include <syscape/sensor.hpp>
#include <syscape/software.hpp>
#include <syscape/storage.hpp>
#include <syscape/user.hpp>
#include <syscape/virtualization.hpp>
#include <syscape/wifi.hpp>

const std::error_category* other_error_category() {
    return &syscape::error_category();
}
syscape::architecture other_architecture() {
    return syscape::target_architecture();
}
bool other_os_backend_callable() {
    const syscape::result<std::string> value = syscape::os::kernel_name();
    return value || value.error() == std::errc::operation_not_supported;
}

bool other_cpu_backend_callable() {
    const syscape::result<std::uint32_t> value =
        syscape::cpu::online_logical_processor_count();
    const syscape::result<std::vector<syscape::cpu::cache_information>>
        caches = syscape::cpu::cache_descriptors();
    const syscape::result<std::vector<std::string>> features =
        syscape::cpu::instruction_set_features();
    static_cast<void>(caches);
    static_cast<void>(features);
    return (value && *value > 0U) ||
           value.error() == std::errc::operation_not_supported;
}

bool other_memory_backend_callable() {
    const syscape::result<std::uint64_t> value =
        syscape::memory::physical_memory_bytes();
    return (value && *value > 0U) ||
           value.error() == std::errc::operation_not_supported;
}

bool other_process_backend_callable() {
    const syscape::result<std::uint32_t> process =
        syscape::process::process_id();
    const syscape::result<std::uint32_t> parent =
        syscape::process::parent_process_id();
    const syscape::result<std::string> executable =
        syscape::process::executable_path();
    const syscape::result<std::vector<std::string>> arguments =
        syscape::process::command_line();
    const syscape::result<std::string> directory =
        syscape::process::working_directory();
    static_cast<void>(parent);
    static_cast<void>(executable);
    static_cast<void>(arguments);
    static_cast<void>(directory);
    return (process && *process > 0U) ||
           process.error() == std::errc::operation_not_supported;
}

bool other_user_backend_callable() {
    const syscape::result<std::uint32_t> real_user =
        syscape::user::real_user_id();
    const syscape::result<std::uint32_t> effective_user =
        syscape::user::effective_user_id();
    const syscape::result<std::uint32_t> real_group =
        syscape::user::real_group_id();
    const syscape::result<std::uint32_t> effective_group =
        syscape::user::effective_group_id();
    const syscape::result<std::vector<std::uint32_t>> groups =
        syscape::user::supplementary_groups();
    const syscape::result<syscape::user::privilege_state> privilege =
        syscape::user::privilege();
    const syscape::result<std::string> name = syscape::user::user_name();
    const syscape::result<std::string> session = syscape::user::login_name();
    const syscape::result<std::string> home =
        syscape::user::home_directory();
    const syscape::result<std::string> shell = syscape::user::shell();
    const syscape::result<std::vector<syscape::user::session_info>> sessions =
        syscape::user::sessions();
    const syscape::result<std::vector<std::string>> logged_in =
        syscape::user::logged_in_users();
    static_cast<void>(effective_user);
    static_cast<void>(real_group);
    static_cast<void>(effective_group);
    static_cast<void>(groups);
    static_cast<void>(privilege);
    static_cast<void>(session);
    static_cast<void>(name);
    static_cast<void>(home);
    static_cast<void>(shell);
    static_cast<void>(sessions);
    static_cast<void>(logged_in);
    return real_user.has_value() ||
           real_user.error() == std::errc::operation_not_supported;
}

bool other_filesystem_backend_callable() {
    const syscape::result<std::vector<syscape::filesystem::mount_entry>>
        mounted = syscape::filesystem::mounts();
    const syscape::result<syscape::filesystem::space_info> capacity =
        syscape::filesystem::space("/");
    const syscape::result<syscape::filesystem::path_length_limit> component =
        syscape::filesystem::max_component_length("/");
    const syscape::result<syscape::filesystem::path_length_limit> path =
        syscape::filesystem::max_path_length("/");
    const syscape::result<std::string> identifier =
        syscape::filesystem::volume_id("/");
    static_cast<void>(capacity);
    static_cast<void>(component);
    static_cast<void>(path);
    static_cast<void>(identifier);
    return (mounted && !mounted->empty()) ||
           mounted.error() == std::errc::operation_not_supported;
}

bool other_network_backend_callable() {
    const syscape::result<std::vector<syscape::network::interface_entry>>
        interfaces = syscape::network::interfaces();
    const auto routes = syscape::network::routes();
    const auto gateways = syscape::network::default_gateways();
    const auto stats = syscape::network::statistics();
    const auto stats_name = syscape::network::statistics("lo");
    const auto stats_idx = syscape::network::statistics(1U);
    static_cast<void>(routes);
    static_cast<void>(gateways);
    static_cast<void>(stats);
    static_cast<void>(stats_name);
    static_cast<void>(stats_idx);
    return interfaces.has_value() != static_cast<bool>(interfaces.error());
}

bool other_locale_backend_callable() {
    const syscape::result<std::string> locale =
        syscape::locale::current_locale();
    const syscape::result<std::string> encoding =
        syscape::locale::text_encoding();
    const syscape::result<std::int32_t> offset =
        syscape::locale::utc_offset_seconds();
    const syscape::result<std::vector<std::string>> languages =
        syscape::locale::preferred_languages();
    const syscape::result<std::string> region =
        syscape::locale::country_region_code();
    const syscape::result<std::string> zone =
        syscape::locale::time_zone_identifier();
    static_cast<void>(encoding);
    static_cast<void>(offset);
    static_cast<void>(languages);
    static_cast<void>(region);
    static_cast<void>(zone);
    return locale.has_value() ||
           locale.error() == std::errc::operation_not_supported;
}

bool other_resource_backend_callable() {
    const syscape::result<std::uint64_t> processes =
        syscape::resource::process_count();
    const syscape::result<syscape::resource::load_snapshot> loads =
        syscape::resource::load_average();
    const syscape::result<std::uint64_t> handles =
        syscape::resource::open_handle_count();
    static_cast<void>(loads);
    static_cast<void>(handles);
    return (processes && *processes > 0U) ||
           processes.error() == std::errc::operation_not_supported ||
           processes.error() == syscape::errc::permission_denied ||
           processes.error() == syscape::errc::temporarily_unavailable;
}

bool other_power_backend_callable() {
    const syscape::result<std::vector<syscape::power::battery_entry>>
        listed = syscape::power::batteries();
    const syscape::result<std::vector<syscape::power::power_source_entry>>
        sources = syscape::power::power_sources();
    const syscape::result<bool> powered =
        syscape::power::external_power_online();
    static_cast<void>(sources);
    static_cast<void>(powered);
    return listed.has_value() ||
           listed.error() == std::errc::operation_not_supported ||
           listed.error() == syscape::errc::not_found ||
           listed.error() == syscape::errc::permission_denied;
}

bool other_environment_backend_callable() {
    const syscape::result<std::string> path =
        syscape::environment::get("PATH");
    const syscape::result<bool> has_path =
        syscape::environment::has("PATH");
    const syscape::result<std::vector<syscape::environment::environment_variable>> vars =
        syscape::environment::environment_variables();
    const syscape::result<std::string> cwd =
        syscape::environment::current_working_directory();
    const syscape::result<std::string> exec =
        syscape::environment::find_executable("ls");
    const syscape::result<std::string> tmp =
        syscape::environment::temp_directory();
    const syscape::result<std::string> home =
        syscape::environment::home_directory();
    const syscape::result<std::string> cfg =
        syscape::environment::config_directory();
    const syscape::result<std::string> data =
        syscape::environment::data_directory();
    const syscape::result<std::string> cache =
        syscape::environment::cache_directory();
    const syscape::result<bool> is_in =
        syscape::environment::is_interactive_stdin();
    const syscape::result<bool> is_out =
        syscape::environment::is_interactive_stdout();
    const syscape::result<bool> is_err =
        syscape::environment::is_interactive_stderr();

    constexpr char list_sep = syscape::environment::path_list_separator();
    constexpr char dir_sep = syscape::environment::directory_separator();
    static_cast<void>(list_sep);
    static_cast<void>(dir_sep);

    static_cast<void>(has_path);
    static_cast<void>(vars);
    static_cast<void>(cwd);
    static_cast<void>(exec);
    static_cast<void>(tmp);
    static_cast<void>(home);
    static_cast<void>(cfg);
    static_cast<void>(data);
    static_cast<void>(cache);
    static_cast<void>(is_in);
    static_cast<void>(is_out);
    static_cast<void>(is_err);
    return path.has_value() ||
           path.error() == std::errc::operation_not_supported;
}

bool other_storage_backend_callable() {
    const syscape::result<std::vector<syscape::storage::drive_entry>>
        listed = syscape::storage::drives();
    const auto parts = syscape::storage::partitions();
    const auto disk_parts = syscape::storage::disk_partitions("disk0");
    const auto single_health = syscape::storage::health("disk0");
    const auto all_health = syscape::storage::all_drive_health();
    static_cast<void>(parts);
    static_cast<void>(disk_parts);
    static_cast<void>(single_health);
    static_cast<void>(all_health);
    return listed.has_value() ||
           listed.error() == std::errc::operation_not_supported ||
           listed.error() == syscape::errc::permission_denied;
}

bool other_hardware_backend_callable() {
    const syscape::result<std::string> manufacturer =
        syscape::hardware::system_manufacturer();
    const syscape::result<syscape::hardware::form_factor> chassis =
        syscape::hardware::chassis_form_factor();
    const syscape::result<std::string> uuid =
        syscape::hardware::hardware_uuid();
    const auto pci = syscape::hardware::pci_devices();
    const auto usb = syscape::hardware::usb_devices();
    const auto mem = syscape::hardware::memory_devices();
    static_cast<void>(chassis);
    static_cast<void>(uuid);
    static_cast<void>(pci);
    static_cast<void>(usb);
    static_cast<void>(mem);
    return manufacturer.has_value() ||
           manufacturer.error() == std::errc::operation_not_supported ||
           manufacturer.error() == syscape::errc::not_found ||
           manufacturer.error() == syscape::errc::permission_denied;
}

bool other_virtualization_backend_callable() {
    const syscape::result<bool> hv =
        syscape::virtualization::is_hypervisor_present();
    const syscape::result<syscape::virtualization::hypervisor_vendor> vendor =
        syscape::virtualization::hypervisor();
    const syscape::result<bool> cont =
        syscape::virtualization::is_container();
    const syscape::result<bool> wsl =
        syscape::virtualization::is_wsl();
    const syscape::result<bool> sb =
        syscape::virtualization::is_sandboxed();
    const syscape::result<syscape::virtualization::cgroup_version> cg_ver =
        syscape::virtualization::cgroup_hierarchy_version();
    const syscape::result<syscape::virtualization::cgroup_info> cg_info =
        syscape::virtualization::current_cgroup();
    const syscape::result<std::vector<syscape::virtualization::namespace_info>> ns_list =
        syscape::virtualization::namespaces();
    const syscape::result<bool> ns_iso =
        syscape::virtualization::is_namespace_isolated();
    static_cast<void>(vendor);
    static_cast<void>(cont);
    static_cast<void>(wsl);
    static_cast<void>(sb);
    static_cast<void>(cg_ver);
    static_cast<void>(cg_info);
    static_cast<void>(ns_list);
    static_cast<void>(ns_iso);
    return hv.has_value() ||
           hv.error() == std::errc::operation_not_supported;
}

bool other_gpu_backend_callable() {
    const syscape::result<std::vector<syscape::gpu::gpu_device>> devs =
        syscape::gpu::devices();
    const syscape::result<std::size_t> count =
        syscape::gpu::device_count();
    const syscape::result<syscape::gpu::gpu_device> primary =
        syscape::gpu::primary_device();
    static_cast<void>(count);
    static_cast<void>(primary);
    return devs.has_value() ||
           devs.error() == std::errc::operation_not_supported;
}

bool other_display_backend_callable() {
    const syscape::result<std::vector<syscape::display::display_info>> disps =
        syscape::display::displays();
    const syscape::result<std::size_t> count =
        syscape::display::display_count();
    const syscape::result<syscape::display::display_info> primary =
        syscape::display::primary_display();
    static_cast<void>(count);
    static_cast<void>(primary);
    return disps.has_value() ||
           disps.error() == std::errc::operation_not_supported;
}

bool other_security_backend_callable() {
    const auto sb = syscape::security::secure_boot();
    const auto is_sb = syscape::security::is_secure_boot_enabled();
    const auto tpm_res = syscape::security::tpm();
    const auto lsm_res = syscape::security::security_modules();
    const auto lock_res = syscape::security::lockdown();
    const auto sip_res = syscape::security::is_sip_enabled();
    const auto aslr_res = syscape::security::aslr();
    const auto vuln_res = syscape::security::cpu_vulnerabilities();
    const auto caps_res = syscape::security::capabilities();
    const auto privs_res = syscape::security::privileges();
    const auto enc_res = syscape::security::volume_encryption("/");
    const auto enc_vols = syscape::security::encrypted_volumes();
    static_cast<void>(is_sb);
    static_cast<void>(tpm_res);
    static_cast<void>(lsm_res);
    static_cast<void>(lock_res);
    static_cast<void>(sip_res);
    static_cast<void>(aslr_res);
    static_cast<void>(vuln_res);
    static_cast<void>(caps_res);
    static_cast<void>(privs_res);
    static_cast<void>(enc_res);
    static_cast<void>(enc_vols);
    return sb.has_value() ||
           sb.error() == std::errc::operation_not_supported ||
           sb.error() == std::errc::permission_denied;
}

bool other_sensor_backend_callable() {
    const auto temps = syscape::sensor::temperatures();
    const auto fans = syscape::sensor::fans();
    const auto zones = syscape::sensor::thermal_zones();
    static_cast<void>(fans);
    static_cast<void>(zones);
    return temps.has_value() ||
           temps.error() == std::errc::operation_not_supported ||
           temps.error() == std::errc::permission_denied;
}

bool other_audio_backend_callable() {
    const auto devs = syscape::audio::devices();
    const auto playbacks = syscape::audio::playback_devices();
    const auto captures = syscape::audio::capture_devices();
    const auto count = syscape::audio::device_count();
    static_cast<void>(playbacks);
    static_cast<void>(captures);
    static_cast<void>(count);
    return devs.has_value() ||
           devs.error() == std::errc::operation_not_supported ||
           devs.error() == std::errc::permission_denied;
}

bool other_input_backend_callable() {
    const auto devs = syscape::input::devices();
    const auto kbds = syscape::input::keyboards();
    const auto mice = syscape::input::mice();
    const auto touches = syscape::input::touch_devices();
    const auto pads = syscape::input::gamepads();
    const auto count = syscape::input::device_count();
    static_cast<void>(kbds);
    static_cast<void>(mice);
    static_cast<void>(touches);
    static_cast<void>(pads);
    static_cast<void>(count);
    return devs.has_value() ||
           devs.error() == std::errc::operation_not_supported ||
           devs.error() == std::errc::permission_denied;
}

bool other_camera_backend_callable() {
    const auto devs = syscape::camera::devices();
    const auto captures = syscape::camera::capture_devices();
    const auto count = syscape::camera::device_count();
    const auto def = syscape::camera::default_device();
    static_cast<void>(captures);
    static_cast<void>(count);
    static_cast<void>(def);
    return devs.has_value() || static_cast<bool>(devs.error());
}

bool other_bluetooth_backend_callable() {
    const auto adapters = syscape::bluetooth::adapters();
    const auto count = syscape::bluetooth::adapter_count();
    const auto def = syscape::bluetooth::default_adapter();
    const auto paired = syscape::bluetooth::paired_devices();
    const auto connected = syscape::bluetooth::connected_devices();
    static_cast<void>(count);
    static_cast<void>(def);
    static_cast<void>(paired);
    static_cast<void>(connected);
    return adapters.has_value() || static_cast<bool>(adapters.error());
}

bool other_wifi_backend_callable() {
    const auto adapters = syscape::wifi::adapters();
    const auto count = syscape::wifi::adapter_count();
    const auto def = syscape::wifi::default_adapter();
    const auto conn = syscape::wifi::current_connection();
    const auto configured = syscape::wifi::configured_networks();
    static_cast<void>(count);
    static_cast<void>(def);
    static_cast<void>(conn);
    static_cast<void>(configured);
    return adapters.has_value() || static_cast<bool>(adapters.error());
}

bool other_printer_backend_callable() {
    const auto printers = syscape::printer::printers();
    const auto count = syscape::printer::printer_count();
    const auto def = syscape::printer::default_printer();
    const auto find = syscape::printer::find_printer("test");
    static_cast<void>(count);
    static_cast<void>(def);
    static_cast<void>(find);
    return printers.has_value() || static_cast<bool>(printers.error());
}

bool other_process_list_backend_callable() {
    const auto procs = syscape::process_list::processes();
    const auto count = syscape::process_list::process_count();
    const auto find = syscape::process_list::find_process(1);
    const auto by_name = syscape::process_list::find_processes_by_name("init");
    static_cast<void>(count);
    static_cast<void>(find);
    static_cast<void>(by_name);
    return procs.has_value() || static_cast<bool>(procs.error());
}

bool other_connection_backend_callable() {
    const auto conns = syscape::connection::connections();
    const auto tcps = syscape::connection::tcp_connections();
    const auto udps = syscape::connection::udp_endpoints();
    const auto listen = syscape::connection::listening_endpoints();
    const auto find = syscape::connection::find_connections_by_process(1);
    static_cast<void>(tcps);
    static_cast<void>(udps);
    static_cast<void>(listen);
    static_cast<void>(find);
    return conns.has_value() || static_cast<bool>(conns.error());
}

bool other_software_backend_callable() {
    const auto svcs = syscape::software::services();
    const auto drvs = syscape::software::loaded_drivers();
    const auto pkgs = syscape::software::installed_packages();
    const auto upds = syscape::software::system_updates();
    const auto rts = syscape::software::installed_runtimes();
    const auto svc = syscape::software::find_service("test");
    const auto drv = syscape::software::find_driver("test");
    const auto pkg = syscape::software::find_package("test");
    static_cast<void>(drvs);
    static_cast<void>(pkgs);
    static_cast<void>(upds);
    static_cast<void>(rts);
    static_cast<void>(svc);
    static_cast<void>(drv);
    static_cast<void>(pkg);
    return svcs.has_value() || static_cast<bool>(svcs.error());
}

bool other_numa_backend_callable() {
    const auto avail = syscape::numa::is_numa_available();
    const auto count = syscape::numa::node_count();
    const auto nodes = syscape::numa::nodes();
    const auto node0 = syscape::numa::node(0U);
    const auto thread_node = syscape::numa::current_thread_node();
    static_cast<void>(avail);
    static_cast<void>(count);
    static_cast<void>(nodes);
    static_cast<void>(node0);
    static_cast<void>(thread_node);
    return avail.has_value() || static_cast<bool>(avail.error());
}

bool other_ipc_backend_callable() {
    const auto shm = syscape::ipc::shared_memory_segments();
    const auto msg = syscape::ipc::message_queues();
    const auto sem = syscape::ipc::semaphore_sets();
    const auto sock = syscape::ipc::local_sockets();
    const auto lim = syscape::ipc::limits();
    static_cast<void>(msg);
    static_cast<void>(sem);
    static_cast<void>(sock);
    static_cast<void>(lim);
    return shm.has_value() || static_cast<bool>(shm.error());
}
