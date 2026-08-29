#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <syscape/display.hpp>
#include <syscape/gpu.hpp>
#include <syscape/hardware.hpp>
#include <syscape/numa.hpp>
#include <syscape/power.hpp>
#include <syscape/storage.hpp>

namespace {

const char* bus_type_name(syscape::storage::bus_type bus) {
    switch (bus) {
    case syscape::storage::bus_type::nvme: return "NVMe";
    case syscape::storage::bus_type::sata: return "SATA";
    case syscape::storage::bus_type::scsi: return "SCSI";
    case syscape::storage::bus_type::sas: return "SAS";
    case syscape::storage::bus_type::ata: return "ATA";
    case syscape::storage::bus_type::atapi: return "ATAPI";
    case syscape::storage::bus_type::usb: return "USB";
    case syscape::storage::bus_type::sd: return "SD";
    case syscape::storage::bus_type::mmc: return "MMC/eMMC";
    case syscape::storage::bus_type::virtual_media: return "Virtual";
    case syscape::storage::bus_type::firewire: return "FireWire";
    case syscape::storage::bus_type::fibre_channel: return "Fibre Channel";
    case syscape::storage::bus_type::iscsi: return "iSCSI";
    case syscape::storage::bus_type::raid: return "RAID";
    case syscape::storage::bus_type::unknown: return "Unknown bus";
    }
    return "Unknown bus";
}

const char* battery_state_name(syscape::power::battery_state state) {
    switch (state) {
    case syscape::power::battery_state::charging: return "Charging";
    case syscape::power::battery_state::discharging: return "Discharging";
    case syscape::power::battery_state::not_charging: return "Not charging (AC connected)";
    case syscape::power::battery_state::full: return "Full";
    case syscape::power::battery_state::unknown: return "Unknown state";
    }
    return "Unknown state";
}

} // namespace

int main() {
    std::cout << "=== Syscape Hardware & Storage Inventory Example ===" << std::endl;

    // Hardware Identity
    std::cout << "\n[System & Motherboard]" << std::endl;
    if (const auto mfr = syscape::hardware::system_manufacturer()) {
        std::cout << "  Manufacturer:  " << *mfr << std::endl;
    }
    if (const auto product = syscape::hardware::system_product_name()) {
        std::cout << "  Product Name:  " << *product << std::endl;
    }
    if (const auto board = syscape::hardware::motherboard_product_name()) {
        std::cout << "  Motherboard:   " << *board << std::endl;
    }
    if (const auto bios = syscape::hardware::firmware_version()) {
        std::cout << "  BIOS/Firmware: " << *bios << std::endl;
    }
    if (const auto uuid = syscape::hardware::hardware_uuid()) {
        std::cout << "  Hardware UUID: " << *uuid << std::endl;
    }

    // Physical Memory Devices (SMBIOS Type 17)
    std::cout << "\n[Physical Memory Slots / Modules]" << std::endl;
    if (const auto mem_devs = syscape::hardware::memory_devices()) {
        for (const auto& dev : *mem_devs) {
            std::cout << "  Slot [" << dev.locator << "]: ";
            if (dev.state == syscape::hardware::memory_device_state::installed && dev.size_bytes) {
                std::cout << (*dev.size_bytes / (1024ULL * 1024ULL * 1024ULL))
                          << " GiB "
                          << (dev.manufacturer ? *dev.manufacturer + " " : "")
                          << (dev.part_number ? *dev.part_number + " " : "");
                if (dev.configured_speed) {
                    std::cout << "@ " << dev.configured_speed->value << " MT/s";
                }
                std::cout << std::endl;
            } else {
                std::cout << "(Empty Slot)" << std::endl;
            }
        }
    }

    // NUMA Topology
    std::cout << "\n[NUMA Topology]" << std::endl;
    if (const auto numa_avail = syscape::numa::is_numa_available()) {
        std::cout << "  Multi-Node:    " << (*numa_avail ? "Yes" : "No (Single node / UMA)") << std::endl;
    }
    if (const auto nodes = syscape::numa::nodes()) {
        for (const auto& n : *nodes) {
            std::cout << "  Node " << n.id << ": "
                      << n.logical_processors.size() << " logical processors";
            if (n.total_memory_bytes) {
                std::cout << ", " << (*n.total_memory_bytes / (1024ULL * 1024ULL * 1024ULL)) << " GiB total";
            }
            std::cout << std::endl;
        }
    }

    // Storage Drives
    std::cout << "\n[Storage Drives]" << std::endl;
    if (const auto drives = syscape::storage::drives()) {
        for (const auto& drive : *drives) {
            std::cout << "  Drive [" << drive.identifier << "]: "
                      << (drive.has_model ? drive.model : "(Unnamed Drive)")
                      << " ("
                      << (drive.has_capacity_bytes
                              ? std::to_string(drive.capacity_bytes / (1024ULL * 1024ULL * 1024ULL)) + " GiB"
                              : "Unknown size")
                      << ", "
                      << bus_type_name(drive.bus)
                      << ", "
                      << (drive.has_rotational
                              ? (drive.rotational ? "Rotational HDD" : "Non-rotational (SSD/Flash)")
                              : "Unknown rotation")
                      << ")" << std::endl;
        }
    }

    // PCI Devices Summary
    std::cout << "\n[PCI / PCIe Device Census]" << std::endl;
    if (const auto pcis = syscape::hardware::pci_devices()) {
        std::cout << "  Total Detected: " << pcis->size() << " devices" << std::endl;
        for (std::size_t i = 0; i < std::min<std::size_t>(pcis->size(), 6); ++i) {
            const auto& dev = (*pcis)[i];
            std::cout << "    [PCI 0x" << std::hex << std::setw(4) << std::setfill('0') << dev.vendor_id
                      << ":0x" << std::setw(4) << std::setfill('0') << dev.device_id << std::dec << "] ";
            if (dev.driver) {
                std::cout << "(Driver: " << *dev.driver << ")";
            }
            std::cout << std::endl;
        }
        if (pcis->size() > 6) {
            std::cout << "    ... and " << (pcis->size() - 6) << " more PCI devices." << std::endl;
        }
    }

    // USB Devices Summary
    std::cout << "\n[USB Device Census]" << std::endl;
    if (const auto usbs = syscape::hardware::usb_devices()) {
        std::cout << "  Total Detected: " << usbs->size() << " devices" << std::endl;
        for (std::size_t i = 0; i < std::min<std::size_t>(usbs->size(), 5); ++i) {
            const auto& dev = (*usbs)[i];
            std::cout << "    "
                      << (dev.product ? *dev.product : "(USB Device)")
                      << " by " << (dev.manufacturer ? *dev.manufacturer : "Vendor 0x" + std::to_string(dev.vendor_id))
                      << std::endl;
        }
        if (usbs->size() > 5) {
            std::cout << "    ... and " << (usbs->size() - 5) << " more USB devices." << std::endl;
        }
    }

    // GPUs
    std::cout << "\n[Graphics Devices (GPUs)]" << std::endl;
    if (const auto gpus = syscape::gpu::devices()) {
        for (const auto& gpu : *gpus) {
            std::cout << "  GPU: " << (gpu.name ? *gpu.name : gpu.vendor_name)
                      << (gpu.is_primary && *gpu.is_primary ? " [Primary]" : "")
                      << std::endl;
            if (gpu.vram_bytes) {
                std::cout << "    VRAM: "
                          << (*gpu.vram_bytes / (1024ULL * 1024ULL))
                          << " MiB" << std::endl;
            }
            if (gpu.driver) {
                std::cout << "    Driver: " << *gpu.driver << std::endl;
            }
        }
    }

    // Displays
    std::cout << "\n[Displays & Monitors]" << std::endl;
    if (const auto displays = syscape::display::displays()) {
        for (const auto& disp : *displays) {
            std::cout << "  Display [" << disp.id << "]: "
                      << (disp.name ? *disp.name : "Display Output") << std::endl;
            if (disp.current_width && disp.current_height) {
                std::cout << "    Resolution: "
                          << *disp.current_width << "x" << *disp.current_height;
                if (disp.refresh_rate_hz) {
                    std::cout << " @ " << *disp.refresh_rate_hz << " Hz";
                }
                std::cout << std::endl;
            }
            if (disp.physical_width_mm && disp.physical_height_mm) {
                std::cout << "    Size: "
                          << *disp.physical_width_mm << "x"
                          << *disp.physical_height_mm << " mm" << std::endl;
            }
        }
    }

    // Power & Batteries
    std::cout << "\n[Power & Battery]" << std::endl;
    if (const auto external = syscape::power::external_power_online()) {
        std::cout << "  AC Power:      "
                  << (*external ? "Online (Connected)" : "Offline (Disconnected)")
                  << std::endl;
    }
    if (const auto batteries = syscape::power::batteries()) {
        for (const auto& bat : *batteries) {
            std::cout << "  Battery [" << bat.identifier << "]: ";
            if (bat.has_charge_percent) {
                std::cout << bat.charge_percent << "% ";
            }
            std::cout << "(" << battery_state_name(bat.state) << ")" << std::endl;
            if (bat.has_health_percent) {
                std::cout << "    Health: " << bat.health_percent << "%"
                          << std::endl;
            }
        }
    }

    return 0;
}
