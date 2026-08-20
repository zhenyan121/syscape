#include <syscape/architecture.hpp>
#include <syscape/capability.hpp>
#include <syscape/execution_environment.hpp>
#include <syscape/toolchain.hpp>

static_assert(syscape::target_data_model_info().pointer_bits > 0U,
              "Pointer width must be observable without hosted facilities");
static_assert(syscape::target_cpp_version() >= 201703L,
              "The language version must be C++17 or later");

int syscape_freestanding_compile_test() {
    const syscape::capability value(syscape::capability_state::unknown);
    return value.recognized() ? 1 : 0;
}
