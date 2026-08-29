#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <syscape/ipc.hpp>
#include <syscape/process_list.hpp>
#include <syscape/resource.hpp>

namespace {

const char* process_state_name(syscape::process_list::process_state state) {
    switch (state) {
    case syscape::process_list::process_state::running: return "RUNNING";
    case syscape::process_list::process_state::sleeping: return "SLEEP";
    case syscape::process_list::process_state::stopped: return "STOPPED";
    case syscape::process_list::process_state::zombie: return "ZOMBIE";
    case syscape::process_list::process_state::unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

} // namespace

int main() {
    std::cout << "=== Syscape Process Monitor & IPC Resources Example ===" << std::endl;

    // System Load & Schedulable Entities
    std::cout << "\n[System Resource Demand & Load Averages]" << std::endl;
    if (const auto load = syscape::resource::load_average()) {
        std::cout << "  Load Averages: 1m=" << std::fixed << std::setprecision(2) << load->one_minute
                  << ", 5m=" << load->five_minute
                  << ", 15m=" << load->fifteen_minute << std::endl;
    }
    if (const auto sched = syscape::resource::scheduler_entities()) {
        std::cout << "  Entities:      " << sched->runnable_entities << " runnable of "
                  << sched->total_entities << " total" << std::endl;
    }
    if (const auto procs = syscape::resource::process_count()) {
        std::cout << "  Total Procs:   " << *procs << std::endl;
    }
    if (const auto threads = syscape::resource::thread_count()) {
        std::cout << "  Total Threads: " << *threads << std::endl;
    }
    if (const auto open_fds = syscape::resource::open_file_count()) {
        std::cout << "  Open Files:    " << *open_fds;
        if (const auto max_fds = syscape::resource::file_descriptor_limit()) {
            std::cout << " (System Limit: " << *max_fds << ")";
        }
        std::cout << std::endl;
    }

    // Active Process Census Table
    std::cout << "\n[Top Active System Processes]" << std::endl;
    if (const auto proc_list = syscape::process_list::processes()) {
        std::cout << "  Total Active Processes: " << proc_list->size() << std::endl;
        std::cout << "  " << std::left << std::setw(8) << "PID"
                  << std::setw(8) << "STATE"
                  << std::setw(12) << "USER"
                  << std::setw(12) << "RSS (MiB)"
                  << "COMMAND" << std::endl;
        std::cout << "  ------------------------------------------------------------" << std::endl;

        for (std::size_t i = 0; i < std::min<std::size_t>(proc_list->size(), 10); ++i) {
            const auto& p = (*proc_list)[i];
            const std::uint64_t rss_mib = p.resident_memory_bytes
                                              ? (*p.resident_memory_bytes / (1024ULL * 1024ULL))
                                              : 0;
            std::cout << "  " << std::left << std::setw(8) << p.pid
                      << std::setw(8) << process_state_name(p.state)
                      << std::setw(12) << (p.user_name ? *p.user_name : "-")
                      << std::setw(12) << (p.resident_memory_bytes ? std::to_string(rss_mib) : "-")
                      << (p.name ? *p.name : "(Unknown)")
                      << std::endl;
        }
        if (proc_list->size() > 10) {
            std::cout << "  ... and " << (proc_list->size() - 10) << " more active processes." << std::endl;
        }
    }

    // Inter-Process Communication (IPC)
    std::cout << "\n[Inter-Process Communication (IPC) Resources]" << std::endl;
    if (const auto shm = syscape::ipc::shared_memory_segments()) {
        std::cout << "  Shared Memory: " << shm->size() << " active segments" << std::endl;
        for (std::size_t i = 0; i < std::min<std::size_t>(shm->size(), 3); ++i) {
            const auto& s = (*shm)[i];
            std::cout << "    [ID " << s.id << "] "
                      << (s.size_bytes / 1024) << " KiB";
            if (s.attached_processes) {
                std::cout << ", " << *s.attached_processes << " attached processes";
            }
            std::cout << std::endl;
        }
    }
    if (const auto msq = syscape::ipc::message_queues()) {
        std::cout << "  Message Queues:" << msq->size() << " active queues" << std::endl;
    }
    if (const auto sems = syscape::ipc::semaphore_sets()) {
        std::cout << "  Semaphores:    " << sems->size() << " semaphore sets" << std::endl;
    }
    if (const auto limits = syscape::ipc::limits()) {
        if (limits->max_message_queues_system) {
            std::cout << "  Max Queues:    " << *limits->max_message_queues_system << std::endl;
        }
        if (limits->max_shared_memory_segment_bytes) {
            std::cout << "  Max SHM Size:  "
                      << (*limits->max_shared_memory_segment_bytes / (1024ULL * 1024ULL))
                      << " MiB" << std::endl;
        }
    }

    return 0;
}
