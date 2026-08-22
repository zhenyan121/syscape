#include <syscape/memory.hpp>
#include <syscape/memory.hpp>

int main() {
    const syscape::result<std::uint64_t> page =
        syscape::memory::page_size_bytes();
    return page && *page == 0U ? 1 : 0;
}
