#include <syscape/cpu.hpp>
#include <syscape/cpu.hpp>

int main() {
    const syscape::cpu::cache_kind kind = syscape::cpu::cache_kind::data;
    static_cast<void>(kind);
    const syscape::result<std::uint32_t> count =
        syscape::cpu::online_logical_processor_count();
    return count && *count == 0U ? 1 : 0;
}
