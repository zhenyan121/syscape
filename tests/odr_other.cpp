#include <system_error>
#include <vector>

#include <syscape/architecture.hpp>
#include <syscape/cpu.hpp>
#include <syscape/environment.hpp>
#include <syscape/error.hpp>
#include <syscape/filesystem.hpp>
#include <syscape/hardware.hpp>
#include <syscape/locale.hpp>
#include <syscape/memory.hpp>
#include <syscape/network.hpp>
#include <syscape/os.hpp>
#include <syscape/power.hpp>
#include <syscape/process.hpp>
#include <syscape/resource.hpp>
#include <syscape/result.hpp>
#include <syscape/storage.hpp>
#include <syscape/user.hpp>

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
    static_cast<void>(effective_user);
    static_cast<void>(real_group);
    static_cast<void>(effective_group);
    static_cast<void>(groups);
    static_cast<void>(privilege);
    static_cast<void>(session);
    static_cast<void>(name);
    static_cast<void>(home);
    static_cast<void>(shell);
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
    static_cast<void>(routes);
    static_cast<void>(gateways);
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
           processes.error() == std::errc::operation_not_supported;
}

bool other_power_backend_callable() {
    const syscape::result<std::vector<syscape::power::battery_entry>>
        listed = syscape::power::batteries();
    const syscape::result<bool> powered =
        syscape::power::external_power_online();
    static_cast<void>(powered);
    return listed.has_value() ||
           listed.error() == std::errc::operation_not_supported ||
           listed.error() == syscape::errc::not_found;
}

bool other_environment_backend_callable() {
    const syscape::result<std::string> path =
        syscape::environment::get("PATH");
    const syscape::result<bool> has_path =
        syscape::environment::has("PATH");
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
    static_cast<void>(has_path);
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
    return listed.has_value() ||
           listed.error() == std::errc::operation_not_supported;
}

bool other_hardware_backend_callable() {
    const syscape::result<std::string> manufacturer =
        syscape::hardware::system_manufacturer();
    const syscape::result<syscape::hardware::form_factor> chassis =
        syscape::hardware::chassis_form_factor();
    const syscape::result<std::string> uuid =
        syscape::hardware::hardware_uuid();
    static_cast<void>(chassis);
    static_cast<void>(uuid);
    return manufacturer.has_value() ||
           manufacturer.error() == std::errc::operation_not_supported;
}
