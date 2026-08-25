#include <cassert>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <csignal>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <syscape/process_list.hpp>
#include <syscape/detail/process_list/linux.hpp>

namespace {

constexpr std::string_view invalid_text_child_mode =
    "--syscape-invalid-text-child";

int run_invalid_text_child(const char* descriptor_text) {
    int descriptor = -1;
    const char* const end = descriptor_text + std::char_traits<char>::length(
                                                   descriptor_text);
    const auto parsed =
        std::from_chars(descriptor_text, end, descriptor);
    if (parsed.ec != std::errc() || parsed.ptr != end || descriptor < 0) {
        return 2;
    }

    char invalid_comm[] = {'b', 'a', 'd', static_cast<char>(0xff), '\0'};
    const char ready =
        ::prctl(PR_SET_NAME, invalid_comm, 0UL, 0UL, 0UL) == 0 ? '1' : '0';
    ssize_t written = -1;
    do {
        written = ::write(descriptor, &ready, 1U);
    } while (written < 0 && errno == EINTR);
    static_cast<void>(::close(descriptor));
    if (written != 1 || ready != '1') {
        return 3;
    }
    for (;;) {
        static_cast<void>(::pause());
    }
}

void stop_child(::pid_t pid) {
    static_cast<void>(::kill(pid, SIGTERM));
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
}

void test_live_process_enumeration() {
    const auto all = syscape::process_list::processes();
    assert(all.has_value());
    assert(!all->empty());

    // Invariant: PIDs must be sorted in ascending order
    for (std::size_t i = 1; i < all->size(); ++i) {
        assert((*all)[i - 1].pid < (*all)[i].pid);
    }

    const ::pid_t my_pid = ::getpid();
    bool found_self = false;
    for (const auto& proc : *all) {
        if (proc.pid == static_cast<std::uint32_t>(my_pid)) {
            found_self = true;
            assert(proc.name.has_value() && !proc.name->empty());
            assert(proc.thread_count.has_value() && *proc.thread_count >= 1U);
            assert(proc.state == syscape::process_list::process_state::running ||
                   proc.state == syscape::process_list::process_state::sleeping);
            break;
        }
    }
    assert(found_self);

    const auto count = syscape::process_list::process_count();
    assert(count.has_value());
    assert(*count >= 1U);
}

void test_live_single_process() {
    const ::pid_t my_pid = ::getpid();
    const auto self_res =
        syscape::process_list::find_process(static_cast<std::uint32_t>(my_pid));
    assert(self_res.has_value());
    assert(self_res->pid == static_cast<std::uint32_t>(my_pid));
    assert(self_res->name.has_value() && !self_res->name->empty());
    assert(self_res->thread_count.has_value() && *self_res->thread_count >= 1U);
    assert(self_res->resident_memory_bytes.has_value() &&
           *self_res->resident_memory_bytes > 0U);
    assert(self_res->virtual_memory_bytes.has_value() &&
           *self_res->virtual_memory_bytes > 0U);

    // Non-existent PID should return not_found
    const auto not_found_res =
        syscape::process_list::find_process(
            (std::numeric_limits<std::uint32_t>::max)());
    assert(!not_found_res);
    assert(not_found_res.error() == syscape::errc::not_found);

    // PID 0 should return not_found
    const auto zero_res = syscape::process_list::find_process(0U);
    assert(!zero_res);
    assert(zero_res.error() == syscape::errc::not_found);
}

void test_live_find_by_name() {
    const auto empty_res = syscape::process_list::find_processes_by_name("");
    assert(empty_res.has_value());
    assert(empty_res->empty());

    const ::pid_t my_pid = ::getpid();
    const auto self =
        syscape::process_list::find_process(static_cast<std::uint32_t>(my_pid));
    assert(self.has_value());
    assert(self->name.has_value());

    const auto matching =
        syscape::process_list::find_processes_by_name(*self->name);
    assert(matching.has_value());
    assert(!matching->empty());

    bool found = false;
    for (const auto& proc : *matching) {
        if (proc.pid == static_cast<std::uint32_t>(my_pid)) {
            found = true;
            break;
        }
    }
    assert(found);

    syscape::process_list::process_entry synthetic;
    synthetic.name = "CaseSensitiveName";
    assert(!syscape::detail::process_list_common::matches_process_name(
        synthetic, "casesensitivename", false));
}

void test_invalid_native_text_is_field_local() {
    int descriptors[2] = {-1, -1};
    assert(::pipe(descriptors) == 0);

    const ::pid_t child = ::fork();
    assert(child >= 0);
    if (child == 0) {
        static_cast<void>(::close(descriptors[0]));
        const std::string descriptor_text = std::to_string(descriptors[1]);
        char invalid_argument[] = {static_cast<char>(0xff), '\0'};
        ::execl("/proc/self/exe", "process_list_linux",
                invalid_text_child_mode.data(), invalid_argument,
                descriptor_text.c_str(), static_cast<char*>(nullptr));
        ::_exit(127);
    }

    static_cast<void>(::close(descriptors[1]));
    char ready = '0';
    ssize_t read_count = -1;
    do {
        read_count = ::read(descriptors[0], &ready, 1U);
    } while (read_count < 0 && errno == EINTR);
    static_cast<void>(::close(descriptors[0]));

    const auto all = syscape::process_list::processes();
    const auto single = syscape::process_list::find_process(
        static_cast<std::uint32_t>(child));

    bool found_child = false;
    bool fields_unavailable = false;
    if (all) {
        for (const auto& process : *all) {
            if (process.pid == static_cast<std::uint32_t>(child)) {
                found_child = true;
                fields_unavailable = !process.name.has_value() &&
                                     !process.command_line.has_value();
                break;
            }
        }
    }

    stop_child(child);

    assert(read_count == 1 && ready == '1');
    assert(all.has_value());
    assert(found_child);
    assert(fields_unavailable);
    assert(single.has_value());
    assert(!single->name.has_value());
    assert(!single->command_line.has_value());
}

void test_synthetic_stat_parsing() {
    using namespace syscape::detail::process_list_backend;

    // Normal stat content
    const std::string normal_stat =
        "1234 (systemd) S 1 1234 1234 0 -1 4194560 1000 200 0 0 50 25 0 0 20 0 "
        "1 0 100000 5000000 500 18446744073709551615 1 1 0 0 0 0 0 0 0 0 0 0 0 "
        "17 0 0 0 0 0 0 0 0 0 0 0 0 0 0";

    const auto parsed = parse_proc_stat_entry(
        normal_stat, 1234, 100, 4096,
        std::chrono::system_clock::time_point{std::chrono::seconds{1700000000}});
    assert(parsed.has_value());
    assert(parsed->pid == 1234);
    assert(parsed->name == "systemd");
    assert(parsed->state == syscape::process_list::process_state::sleeping);
    assert(parsed->ppid == 1);
    assert(parsed->thread_count == 1);
    assert(parsed->user_cpu_time == std::chrono::nanoseconds{500000000});
    assert(parsed->kernel_cpu_time == std::chrono::nanoseconds{250000000});
    assert(parsed->virtual_memory_bytes == 5000000);
    assert(parsed->resident_memory_bytes == 500 * 4096);
    assert(parsed->priority == 20);
    assert(parsed->start_time.has_value());

    // Stat content with spaces and nested parentheses in comm
    const std::string complex_comm_stat =
        "5678 (Web (Tab) Worker) R 1234 5678 5678 0 -1 0 0 0 0 0 100 200 0 0 -5 0 "
        "4 0 200000 8000000 1000";

    const auto parsed_complex = parse_proc_stat_entry(
        complex_comm_stat, 5678, 100, 4096, std::nullopt);
    assert(parsed_complex.has_value());
    assert(parsed_complex->pid == 5678);
    assert(parsed_complex->name == "Web (Tab) Worker");
    assert(parsed_complex->state == syscape::process_list::process_state::running);
    assert(parsed_complex->ppid == 1234);
    assert(parsed_complex->thread_count == 4);
    assert(parsed_complex->priority == -5);

    const auto unavailable_units = parse_proc_stat_entry(
        normal_stat, 1234, 0, 0, std::chrono::system_clock::time_point{});
    assert(unavailable_units.has_value());
    assert(!unavailable_units->user_cpu_time.has_value());
    assert(!unavailable_units->kernel_cpu_time.has_value());
    assert(!unavailable_units->start_time.has_value());
    assert(!unavailable_units->resident_memory_bytes.has_value());

    // Zombie state
    const std::string zombie_stat =
        "9999 (defunct_app) Z 1 9999 9999 0 -1 0 0 0 0 0 0 0 0 0 20 0 1 0 0 0 0";
    const auto parsed_zombie =
        parse_proc_stat_entry(zombie_stat, 9999, 100, 4096, std::nullopt);
    assert(parsed_zombie.has_value());
    assert(parsed_zombie->state == syscape::process_list::process_state::zombie);

    // Stopped state
    const std::string stopped_stat =
        "8888 (gdb_target) T 1 8888 8888 0 -1 0 0 0 0 0 0 0 0 0 20 0 1 0 0 0 0";
    const auto parsed_stopped =
        parse_proc_stat_entry(stopped_stat, 8888, 100, 4096, std::nullopt);
    assert(parsed_stopped.has_value());
    assert(parsed_stopped->state == syscape::process_list::process_state::stopped);

    // Malformed stat lines
    assert(!parse_proc_stat_entry("not_a_stat_line", 1, 100, 4096, std::nullopt));
    assert(!parse_proc_stat_entry("1 ()", 1, 100, 4096, std::nullopt));
    assert(!parse_proc_stat_entry("1 (name) S 1 2 3", 1, 100, 4096, std::nullopt));
    assert(skippable_process_error(syscape::make_error_code(
        syscape::errc::malformed_data)));
}

void test_synthetic_status_parsing() {
    using namespace syscape::detail::process_list_backend;

    const std::string status_content =
        "Name:\ttest_service\n"
        "Umask:\t0022\n"
        "State:\tS (sleeping)\n"
        "Tgid:\t4321\n"
        "Ngid:\t0\n"
        "Pid:\t4321\n"
        "PPid:\t1\n"
        "Uid:\t1005\t1005\t1005\t1005\n"
        "Gid:\t2005\t2005\t2005\t2005\n"
        "FDSize:\t64\n"
        "Groups:\t100 2005\n"
        "Threads:\t2\n";

    syscape::process_list::process_entry entry;
    parse_proc_status(status_content, entry);

    assert(entry.name == "test_service");
    assert(entry.uid.has_value() && *entry.uid == 1005);
    assert(entry.gid.has_value() && *entry.gid == 2005);
}

void test_synthetic_cmdline_parsing() {
    using namespace syscape::detail::process_list_backend;

    const std::string raw_cmdline = std::string("my_program\0--flag\0value\0", 24);
    const auto args = parse_proc_cmdline(raw_cmdline);
    assert(args.has_value());
    assert(args->size() == 3);
    assert((*args)[0] == "my_program");
    assert((*args)[1] == "--flag");
    assert((*args)[2] == "value");

    const auto empty_args = parse_proc_cmdline("");
    assert(empty_args.has_value());
    assert(empty_args->empty());

    const std::string invalid_utf8("\xff\0", 2);
    const auto invalid_args = parse_proc_cmdline(invalid_utf8);
    assert(!invalid_args);
    assert(invalid_args.error() == syscape::errc::invalid_encoding);

    const auto truncated_args = parse_proc_cmdline("missing-terminator");
    assert(!truncated_args);
    assert(truncated_args.error() == syscape::errc::malformed_data);
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 4 && argv[1] == invalid_text_child_mode) {
        return run_invalid_text_child(argv[3]);
    }
    test_live_process_enumeration();
    test_live_single_process();
    test_live_find_by_name();
    test_invalid_native_text_is_field_local();
    test_synthetic_stat_parsing();
    test_synthetic_status_parsing();
    test_synthetic_cmdline_parsing();
    return 0;
}
