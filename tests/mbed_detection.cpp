#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::mbed,
              "Mbed OS must select mbed");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "Mbed OS must select the rtos execution environment");

int main() {
    return 0;
}
