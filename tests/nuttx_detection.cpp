#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::nuttx,
              "__NuttX__ must select NuttX");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "NuttX must select the RTOS execution environment");

int main() {
    return 0;
}
