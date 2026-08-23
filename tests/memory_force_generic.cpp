#include <cstdint>
#include <system_error>

#include <syscape/memory.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    return unsupported(syscape::memory::page_size_bytes()) &&
                   unsupported(syscape::memory::physical_memory_bytes()) &&
                   unsupported(syscape::memory::available_memory_bytes()) &&
                   unsupported(syscape::memory::swap_status()) &&
                   unsupported(syscape::memory::commit_status()) &&
                   unsupported(syscape::memory::huge_page_size_bytes()) &&
                   unsupported(syscape::memory::huge_page_pool_status()) &&
                   unsupported(syscape::memory::memory_load_percent()) &&
                   unsupported(syscape::memory::memory_pressure())
               ? 0
               : 1;
}
