#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <syscape/process.hpp>
#include <syscape/process.hpp>

int main() {
    const syscape::result<std::uint32_t> id =
        syscape::process::process_id();
    const syscape::result<std::uint32_t> parent =
        syscape::process::parent_process_id();
    const syscape::result<std::string> path =
        syscape::process::executable_path();
    const syscape::result<std::vector<std::string>> arguments =
        syscape::process::command_line();
    const syscape::result<std::string> directory =
        syscape::process::working_directory();
    const syscape::result<syscape::process::cpu_times> cpu =
        syscape::process::cpu_time();
    const syscape::result<std::chrono::system_clock::time_point> started =
        syscape::process::start_time();
    const syscape::result<syscape::process::memory_usage_info> memory =
        syscape::process::memory_usage();
    const syscape::result<std::uint32_t> threads =
        syscape::process::thread_count();

    static_cast<void>(parent);
    static_cast<void>(path);
    static_cast<void>(arguments);
    static_cast<void>(directory);
    static_cast<void>(cpu);
    static_cast<void>(started);
    static_cast<void>(memory);
    static_cast<void>(threads);
    return id && *id == 0U ? 1 : 0;
}
