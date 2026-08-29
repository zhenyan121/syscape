#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <syscape/architecture.hpp>
#include <syscape/cpu.hpp>
#include <syscape/environment.hpp>
#include <syscape/memory.hpp>
#include <syscape/os.hpp>
#include <syscape/process.hpp>
#include <syscape/user.hpp>

int main() {
    std::cout << "=== Syscape System Overview Example ===" << std::endl;

    // Architecture & Target Facts (Freestanding C++11 Foundation)
    std::cout << "\n[Target Architecture]" << std::endl;
    std::cout << "  Architecture:  "
              << syscape::architecture_name(syscape::target_architecture())
              << std::endl;
    std::cout << "  Pointer Width: "
              << syscape::target_data_model_info().pointer_bits
              << " bits" << std::endl;
    std::cout << "  Byte Order:    "
              << syscape::byte_order_name(syscape::target_byte_order())
              << std::endl;

    // Operating System (Hosted Full C++17)
    std::cout << "\n[Operating System]" << std::endl;
    if (const auto name = syscape::os::product_name()) {
        std::cout << "  Name:          " << *name << std::endl;
    }
    if (const auto version = syscape::os::product_version()) {
        std::cout << "  Version:       " << *version << std::endl;
    }
    if (const auto kernel_name = syscape::os::kernel_name()) {
        std::cout << "  Kernel:        " << *kernel_name;
        if (const auto kernel_version = syscape::os::kernel_version()) {
            std::cout << " " << *kernel_version;
        }
        std::cout << std::endl;
    }
    if (const auto hostname = syscape::os::host_name()) {
        std::cout << "  Hostname:      " << *hostname << std::endl;
    }
    if (const auto uptime = syscape::os::uptime()) {
        std::cout << "  Uptime:        " << (uptime->count() / 1000) << " seconds"
                  << std::endl;
    }

    // CPU Information
    std::cout << "\n[CPU / Processor]" << std::endl;
    if (const auto vendors = syscape::cpu::vendor_identifiers()) {
        for (const auto& v : *vendors) {
            std::cout << "  Vendor:        " << v << std::endl;
        }
    }
    if (const auto models = syscape::cpu::model_names()) {
        for (const auto& m : *models) {
            std::cout << "  Model:         " << m << std::endl;
        }
    }
    if (const auto physical = syscape::cpu::online_physical_core_count()) {
        std::cout << "  Physical Cores:" << *physical << std::endl;
    }
    if (const auto logical = syscape::cpu::online_logical_processor_count()) {
        std::cout << "  Logical Cores: " << *logical << std::endl;
    }
    if (const auto min_khz = syscape::cpu::minimum_frequency_khz()) {
        std::cout << "  Min Clock:     " << (*min_khz / 1000) << " MHz"
                  << std::endl;
    }
    if (const auto max_khz = syscape::cpu::maximum_frequency_khz()) {
        std::cout << "  Max Clock:     " << (*max_khz / 1000) << " MHz"
                  << std::endl;
    }

    // Memory Information
    std::cout << "\n[Memory]" << std::endl;
    if (const auto total = syscape::memory::physical_memory_bytes()) {
        std::cout << "  Total Physical: "
                  << (*total / (1024ULL * 1024ULL * 1024ULL)) << " GiB ("
                  << *total << " bytes)" << std::endl;
    }
    if (const auto avail = syscape::memory::available_memory_bytes()) {
        std::cout << "  Available:      "
                  << (*avail / (1024ULL * 1024ULL * 1024ULL)) << " GiB ("
                  << *avail << " bytes)" << std::endl;
    }
    if (const auto util = syscape::memory::memory_load_percent()) {
        std::cout << "  Utilization:    " << static_cast<int>(*util) << "%"
                  << std::endl;
    }
    if (const auto page = syscape::memory::page_size_bytes()) {
        std::cout << "  Page Size:      " << *page << " bytes" << std::endl;
    }

    // Process & User Context
    std::cout << "\n[Current Execution Context]" << std::endl;
    if (const auto pid = syscape::process::process_id()) {
        std::cout << "  PID:            " << *pid << std::endl;
    }
    if (const auto ppid = syscape::process::parent_process_id()) {
        std::cout << "  Parent PID:     " << *ppid << std::endl;
    }
    if (const auto exe = syscape::process::executable_path()) {
        std::cout << "  Executable:     " << *exe << std::endl;
    }
    if (const auto cwd = syscape::environment::current_working_directory()) {
        std::cout << "  Working Dir:    " << *cwd << std::endl;
    }
    if (const auto user = syscape::user::user_name()) {
        std::cout << "  User Name:      " << *user << std::endl;
    }
    if (const auto elev = syscape::user::privilege()) {
        std::cout << "  Elevated:       "
                  << (*elev == syscape::user::privilege_state::privileged
                          ? "Yes (Admin/Root)"
                          : "No (Standard)")
                  << std::endl;
    }

    return 0;
}
