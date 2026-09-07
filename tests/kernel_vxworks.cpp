#include <iostream>
#include <string>

#include <syscape/audio.hpp>
#include <syscape/connection.hpp>
#include <syscape/cpu.hpp>
#include <syscape/environment.hpp>
#include <syscape/filesystem.hpp>
#include <syscape/hardware.hpp>
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

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_kernel_mode_queries() {
    // OS queries
    const auto prod = syscape::os::product_name();
    expect(prod && *prod == "VxWorks", "product name must be VxWorks");

    // Process queries in kernel mode must return not_supported
    expect(syscape::process::process_id().error() ==
               syscape::errc::not_supported,
           "process_id must report not_supported in kernel mode");
    expect(syscape::process::parent_process_id().error() ==
               syscape::errc::not_supported,
           "parent_process_id must report not_supported in kernel mode");
    expect(syscape::process::working_directory().error() ==
               syscape::errc::not_supported,
           "working_directory must report not_supported in kernel mode");
    expect(syscape::process::cpu_time().error() == syscape::errc::not_supported,
           "cpu_time must report not_supported in kernel mode");
    expect(syscape::process::priority().error() == syscape::errc::not_supported,
           "priority must report not_supported in kernel mode");
    expect(syscape::process_list::processes().error() ==
               syscape::errc::not_supported,
           "processes must report not_supported in kernel mode");

    // User queries in kernel mode must return not_supported
    expect(syscape::user::real_user_id().error() ==
               syscape::errc::not_supported,
           "real_user_id must report not_supported in kernel mode");
    expect(syscape::user::effective_user_id().error() ==
               syscape::errc::not_supported,
           "effective_user_id must report not_supported in kernel mode");
    expect(syscape::user::real_group_id().error() ==
               syscape::errc::not_supported,
           "real_group_id must report not_supported in kernel mode");
    expect(syscape::user::effective_group_id().error() ==
               syscape::errc::not_supported,
           "effective_group_id must report not_supported in kernel mode");
    expect(syscape::user::privilege().error() == syscape::errc::not_supported,
           "privilege must report not_supported in kernel mode");
    expect(syscape::user::user_name().error() == syscape::errc::not_supported,
           "user_name must report not_supported in kernel mode");
    expect(syscape::user::home_directory().error() ==
               syscape::errc::not_supported,
           "home_directory must report not_supported in kernel mode");
    expect(syscape::user::shell().error() == syscape::errc::not_supported,
           "shell must report not_supported in kernel mode");
    expect(syscape::user::supplementary_groups().error() ==
               syscape::errc::not_supported,
           "supplementary_groups must report not_supported in kernel mode");

    // Filesystem queries in kernel mode must return not_supported
    expect(syscape::filesystem::space("/").error() ==
               syscape::errc::not_supported,
           "space must report not_supported in kernel mode");
    expect(syscape::filesystem::volume_id("/").error() ==
               syscape::errc::not_supported,
           "volume_id must report not_supported in kernel mode");
    expect(syscape::filesystem::max_component_length("/").error() ==
               syscape::errc::not_supported,
           "max_component_length must report not_supported in kernel mode");
    expect(syscape::filesystem::max_path_length("/").error() ==
               syscape::errc::not_supported,
           "max_path_length must report not_supported in kernel mode");

    // Network queries in kernel mode must return not_supported
    expect(syscape::network::interfaces().error() ==
               syscape::errc::not_supported,
           "interfaces must report not_supported in kernel mode");
    expect(syscape::network::routes().error() == syscape::errc::not_supported,
           "routes must report not_supported in kernel mode");

    // Environment queries in kernel mode must return not_supported
    expect(
        syscape::environment::current_working_directory().error() ==
            syscape::errc::not_supported,
        "current_working_directory must report not_supported in kernel mode");
    expect(syscape::environment::temp_directory().error() ==
               syscape::errc::not_supported,
           "temp_directory must report not_supported in kernel mode");
    expect(syscape::environment::home_directory().error() ==
               syscape::errc::not_supported,
           "home_directory must report not_supported in kernel mode");
    expect(syscape::environment::environment_variables().error() ==
               syscape::errc::not_supported,
           "environment_variables must report not_supported in kernel mode");

    // Memory and CPU queries in kernel mode
    expect(syscape::memory::page_size_bytes().error() ==
               syscape::errc::not_supported,
           "page_size_bytes must report not_supported in kernel mode");
    expect(syscape::cpu::online_logical_processor_count().error() ==
               syscape::errc::not_supported,
           "online_logical_processor_count must report not_supported in kernel "
           "mode");

    // Resource queries in kernel mode
    expect(syscape::resource::file_descriptor_limit().error() ==
               syscape::errc::not_supported,
           "file_descriptor_limit must report not_supported in kernel mode");
}

} // namespace

int main() {
    test_kernel_mode_queries();
    return failures == 0 ? 0 : 1;
}
