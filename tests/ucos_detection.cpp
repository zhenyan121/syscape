#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::ucos,
              "UCOS must select uC/OS");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "uC/OS must select the rtos execution environment");

int main() {
    return 0;
}
