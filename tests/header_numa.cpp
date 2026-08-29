#include <syscape/numa.hpp>
#include <syscape/numa.hpp>

#include <cstdint>
#include <system_error>

namespace {

template <typename Query>
bool honest(const Query& query) {
    try {
        const auto value = query();
        static_cast<void>(value);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

int main() {
    return honest(syscape::numa::is_numa_available) &&
                   honest(syscape::numa::node_count) &&
                   honest(syscape::numa::nodes) &&
                   honest([] { return syscape::numa::node(0U); }) &&
                   honest(syscape::numa::current_thread_node)
               ? 0
               : 1;
}
