#include <syscape/architecture.hpp>
#include <syscape/board.hpp>
#include <syscape/capability.hpp>
#include <syscape/toolchain.hpp>

syscape::architecture other_minimal_architecture();
const char* other_minimal_compiler_name();
bool other_minimal_capability();
syscape::board_family other_minimal_board_family();
const char* other_minimal_board_family_name();
const syscape::board_provider* other_minimal_current_board_provider();

int main() {
    if (other_minimal_architecture() != syscape::target_architecture()) {
        return 1;
    }
    if (other_minimal_compiler_name() == nullptr) {
        return 2;
    }
    if (!other_minimal_capability()) {
        return 3;
    }
    if (other_minimal_board_family() != syscape::target_board_family()) {
        return 4;
    }
    if (other_minimal_board_family_name() == nullptr) {
        return 5;
    }

    const syscape::board_provider test_prov = {nullptr, nullptr, nullptr,
                                               nullptr, nullptr, nullptr,
                                               nullptr, nullptr, nullptr};
    syscape::register_board_provider(&test_prov);
    if (other_minimal_current_board_provider() != &test_prov) {
        return 6;
    }
    syscape::clear_board_provider();
    if (other_minimal_current_board_provider() != nullptr) {
        return 7;
    }

    return 0;
}
