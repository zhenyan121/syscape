#include <syscape/architecture.hpp>
#include <syscape/capability.hpp>
#include <syscape/toolchain.hpp>

syscape::architecture other_minimal_architecture();
const char* other_minimal_compiler_name();
bool other_minimal_capability();

int main() {
    if (other_minimal_architecture() != syscape::target_architecture()) {
        return 1;
    }
    if (other_minimal_compiler_name() == nullptr) {
        return 2;
    }
    return other_minimal_capability() ? 0 : 3;
}
