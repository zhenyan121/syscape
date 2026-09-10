#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::fuchsia,
              "__Fuchsia__ must select Fuchsia");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::sandboxed,
              "Fuchsia must select the sandboxed execution environment");

int main() {
    return 0;
}
