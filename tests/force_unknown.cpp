#include <syscape/architecture.hpp>
#include <syscape/execution_environment.hpp>
#include <syscape/toolchain.hpp>

int main() {
    if (syscape::target_architecture() != syscape::architecture::unknown) {
        return 1;
    }
    if (syscape::target_byte_order() != syscape::byte_order::unknown) {
        return 2;
    }
    if (syscape::target_compiler() != syscape::compiler::unknown) {
        return 3;
    }
    if (syscape::target_standard_library() != syscape::standard_library::unknown) {
        return 4;
    }
    if (syscape::target_operating_system() != syscape::operating_system::unknown) {
        return 5;
    }
    if (syscape::target_execution_environment() !=
        syscape::execution_environment::unknown) {
        return 6;
    }
    return 0;
}
