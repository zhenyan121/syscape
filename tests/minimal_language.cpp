#include <climits>

#include <syscape/architecture.hpp>
#include <syscape/capability.hpp>
#include <syscape/execution_environment.hpp>
#include <syscape/toolchain.hpp>

static_assert(syscape::target_cpp_version() >= 201103L,
              "The minimal profile requires C++11 or later");
static_assert(syscape::target_data_model_info().pointer_bits ==
                  sizeof(void*) * CHAR_BIT,
              "Pointer width must be a compile-time target fact");

#if SYSCAPE_DETAIL_CPLUSPLUS >= 201402L
static_assert(syscape::architecture_name(syscape::architecture::unknown)[0] ==
                  'u',
              "Name helpers must be constexpr in C++14 and later");
static_assert(syscape::target_data_model() != syscape::data_model::unknown,
              "Data-model classification must be constexpr in C++14 and later");
#endif

int main() {
    const syscape::capability available(syscape::capability_state::available);
    if (!available || !available.recognized()) {
        return 1;
    }
    if (syscape::architecture_name(syscape::target_architecture()) == nullptr) {
        return 2;
    }
    if (syscape::byte_order_name(syscape::target_byte_order()) == nullptr) {
        return 3;
    }
    if (syscape::data_model_name(syscape::target_data_model()) == nullptr) {
        return 4;
    }
    if (syscape::compiler_name(syscape::target_compiler()) == nullptr) {
        return 5;
    }
    if (syscape::standard_library_name(syscape::target_standard_library()) ==
        nullptr) {
        return 6;
    }
    if (syscape::operating_system_name(syscape::target_operating_system()) ==
        nullptr) {
        return 7;
    }
    return syscape::execution_environment_name(
               syscape::target_execution_environment()) == nullptr
               ? 8
               : 0;
}
