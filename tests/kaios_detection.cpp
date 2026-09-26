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
