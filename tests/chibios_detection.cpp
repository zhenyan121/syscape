#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::chibios,
              "ChibiOS must select chibios");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "ChibiOS must select the rtos execution environment");

int main() {
    return 0;
}
