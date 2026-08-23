#include <cstdint>
#include <system_error>

#include <syscape/resource.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    return unsupported(syscape::resource::load_average()) &&
                   unsupported(syscape::resource::scheduler_entities()) &&
                   unsupported(syscape::resource::process_count()) &&
                   unsupported(syscape::resource::thread_count()) &&
                   unsupported(syscape::resource::open_file_count()) &&
                   unsupported(syscape::resource::open_handle_count()) &&
                   unsupported(syscape::resource::file_descriptor_limit())
               ? 0
               : 1;
}
