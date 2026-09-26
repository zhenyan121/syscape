// Tests the macro protocol contract for KaiOS compile-target identification.
// Standard toolchains on mobile Linux platforms do not guarantee
// compiler-defined macros; platform selection is established via build-system
// or SDK defines.
#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::kaios,
              "KaiOS must select kaios operating system");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::sandboxed,
              "KaiOS must select sandboxed execution environment");

int main() {
    return 0;
}
