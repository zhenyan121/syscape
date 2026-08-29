#include <cstdint>
#include <system_error>

#include <syscape/numa.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    return unsupported(syscape::numa::is_numa_available()) &&
                   unsupported(syscape::numa::node_count()) &&
                   unsupported(syscape::numa::nodes()) &&
                   unsupported(syscape::numa::node(0U)) &&
                   unsupported(syscape::numa::current_thread_node())
               ? 0
               : 1;
}
