#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <syscape/software.hpp>

namespace {

const char* service_state_name(syscape::software::service_state state) {
    switch (state) {
    case syscape::software::service_state::running: return "RUNNING";
    case syscape::software::service_state::stopped: return "STOPPED";
    case syscape::software::service_state::paused: return "PAUSED";
    case syscape::software::service_state::starting: return "STARTING";
    case syscape::software::service_state::stopping: return "STOPPING";
    case syscape::software::service_state::unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* runtime_kind_name(syscape::software::runtime_kind kind) {
    switch (kind) {
    case syscape::software::runtime_kind::python: return "Python";
    case syscape::software::runtime_kind::nodejs: return "Node.js";
    case syscape::software::runtime_kind::java: return "Java";
    case syscape::software::runtime_kind::dotnet: return ".NET";
    case syscape::software::runtime_kind::rust: return "Rust";
    case syscape::software::runtime_kind::golang: return "Go";
    case syscape::software::runtime_kind::ruby: return "Ruby";
    case syscape::software::runtime_kind::php: return "PHP";
    case syscape::software::runtime_kind::unknown: return "Runtime";
    }
    return "Runtime";
}

} // namespace

int main() {
    std::cout << "=== Syscape Software & Services Inventory Example ===" << std::endl;

    // Discovered Language Runtimes & Toolchains
    std::cout << "\n[Discovered Language Runtimes]" << std::endl;
    if (const auto runtimes = syscape::software::installed_runtimes()) {
        std::cout << "  Total Runtimes: " << runtimes->size() << std::endl;
        for (const auto& r : *runtimes) {
            std::cout << "  - " << runtime_kind_name(r.kind) << ": "
                      << r.name << " v" << r.version
                      << " (" << r.installation_path << ")" << std::endl;
        }
    }

    // System Services & Daemons
    std::cout << "\n[System Services & Background Daemons]" << std::endl;
    if (const auto svcs = syscape::software::services()) {
        std::cout << "  Total Services: " << svcs->size() << std::endl;
        for (std::size_t i = 0; i < std::min<std::size_t>(svcs->size(), 6); ++i) {
            const auto& s = (*svcs)[i];
            std::cout << "  [" << service_state_name(s.state) << "] "
                      << s.name
                      << (s.display_name ? " (" + *s.display_name + ")" : "");
            if (s.pid) {
                std::cout << " [PID " << *s.pid << "]";
            }
            std::cout << std::endl;
        }
        if (svcs->size() > 6) {
            std::cout << "  ... and " << (svcs->size() - 6) << " more services." << std::endl;
        }
    }

    // Loaded Kernel Drivers / Modules
    std::cout << "\n[Loaded Kernel Drivers & Modules]" << std::endl;
    if (const auto drivers = syscape::software::loaded_drivers()) {
        std::cout << "  Total Drivers: " << drivers->size() << std::endl;
        for (std::size_t i = 0; i < std::min<std::size_t>(drivers->size(), 6); ++i) {
            const auto& d = (*drivers)[i];
            std::cout << "  - " << d.name;
            if (d.size_bytes) {
                std::cout << " (" << (*d.size_bytes / 1024) << " KiB)";
            }
            if (d.use_count) {
                std::cout << ", used by " << *d.use_count << " instances";
            }
            std::cout << std::endl;
        }
        if (drivers->size() > 6) {
            std::cout << "  ... and " << (drivers->size() - 6) << " more kernel modules." << std::endl;
        }
    }

    // Installed Packages / Applications
    std::cout << "\n[Installed Software Packages]" << std::endl;
    if (const auto pkgs = syscape::software::installed_packages()) {
        std::cout << "  Total Installed Packages: " << pkgs->size() << std::endl;
        for (std::size_t i = 0; i < std::min<std::size_t>(pkgs->size(), 6); ++i) {
            const auto& p = (*pkgs)[i];
            std::cout << "  - " << p.name;
            if (p.version) {
                std::cout << " v" << *p.version;
            }
            if (p.publisher) {
                std::cout << " (" << *p.publisher << ")";
            }
            std::cout << std::endl;
        }
        if (pkgs->size() > 6) {
            std::cout << "  ... and " << (pkgs->size() - 6) << " more packages." << std::endl;
        }
    } else {
        std::cout << "  (No package database accessible)" << std::endl;
    }

    // Pending System Updates
    std::cout << "\n[Pending System Software Updates]" << std::endl;
    if (const auto updates = syscape::software::system_updates()) {
        std::cout << "  Pending Updates: " << updates->size() << std::endl;
        for (const auto& u : *updates) {
            std::cout << "  - [" << u.identifier << "] " << u.title;
            if (u.version) {
                std::cout << " -> v" << *u.version;
            }
            if (u.requires_reboot) {
                std::cout << " [Reboot Required]";
            }
            std::cout << std::endl;
        }
    } else {
        std::cout << "  (No pending reboot/update records reported by platform)" << std::endl;
    }

    return 0;
}
