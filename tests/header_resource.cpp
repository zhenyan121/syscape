#include <cstdint>
#include <string>

#include <syscape/resource.hpp>
#include <syscape/resource.hpp>

int main() {
    const syscape::result<syscape::resource::load_snapshot> loads =
        syscape::resource::load_average();
    const syscape::result<syscape::resource::scheduling_snapshot> entities =
        syscape::resource::scheduler_entities();
    const syscape::result<std::uint64_t> processes =
        syscape::resource::process_count();
    const syscape::result<std::uint64_t> threads =
        syscape::resource::thread_count();
    const syscape::result<std::uint64_t> open_files =
        syscape::resource::open_file_count();
    const syscape::result<std::uint64_t> handles =
        syscape::resource::open_handle_count();
    const syscape::result<std::uint64_t> limit =
        syscape::resource::file_descriptor_limit();

    static_cast<void>(loads);
    static_cast<void>(entities);
    static_cast<void>(processes);
    static_cast<void>(threads);
    static_cast<void>(open_files);
    static_cast<void>(handles);
    static_cast<void>(limit);
    return 0;
}
