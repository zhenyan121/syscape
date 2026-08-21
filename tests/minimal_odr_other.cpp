#include <syscape/architecture.hpp>
#include <syscape/capability.hpp>
#include <syscape/toolchain.hpp>

syscape::architecture other_minimal_architecture() {
    return syscape::target_architecture();
}

const char* other_minimal_compiler_name() {
    return syscape::compiler_name(syscape::target_compiler());
}

bool other_minimal_capability() {
    return syscape::capability(syscape::capability_state::available).available();
}
