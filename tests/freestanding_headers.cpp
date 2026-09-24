#include <syscape/architecture.hpp>
#include <syscape/board.hpp>
#include <syscape/capability.hpp>
#include <syscape/execution_environment.hpp>
#include <syscape/toolchain.hpp>

static_assert(syscape::target_data_model_info().pointer_bits > 0U,
              "Pointer width must be observable without hosted facilities");
static_assert(syscape::target_cpp_version() >= 201103L,
              "The language version must be C++11 or later");

int syscape_freestanding_compile_test() {
    const syscape::capability value(syscape::capability_state::unknown);
    const syscape::board_family family = syscape::target_board_family();
    return value.recognized() || syscape::board_family_name(family) == nullptr
               ? 1
               : 0;
}
