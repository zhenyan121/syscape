#include <syscape/architecture.hpp>
#include <syscape/board.hpp>
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

syscape::board_family other_minimal_board_family() {
    return syscape::target_board_family();
}

const char* other_minimal_board_family_name() {
    return syscape::board_family_name(syscape::target_board_family());
}

const syscape::board_provider* other_minimal_current_board_provider() {
    return syscape::current_board_provider();
}

syscape::toolchain_version other_minimal_compiler_version() {
    return syscape::target_compiler_version();
}
