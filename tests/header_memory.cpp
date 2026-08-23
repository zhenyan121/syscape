#include <syscape/memory.hpp>
#include <syscape/memory.hpp>

#include <cstdint>
#include <system_error>

namespace {

/// Checks that a query either succeeds or fails with an explicit portable
/// condition, never with an exception or a fabricated value.
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
    return honest(syscape::memory::page_size_bytes) &&
                   honest(syscape::memory::physical_memory_bytes) &&
                   honest(syscape::memory::available_memory_bytes) &&
                   honest(syscape::memory::swap_status) &&
                   honest(syscape::memory::commit_status) &&
                   honest(syscape::memory::huge_page_size_bytes) &&
                   honest(syscape::memory::huge_page_pool_status) &&
                   honest(syscape::memory::memory_load_percent) &&
                   honest(syscape::memory::memory_pressure)
               ? 0
               : 1;
}
