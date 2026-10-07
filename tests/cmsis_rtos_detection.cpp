#include <cassert>
#include <cstring>
#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::cmsis_rtos,
              "CMSIS-RTOS must select cmsis_rtos");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "CMSIS-RTOS must select the rtos execution environment");

int main() {
    assert(std::strcmp(syscape::operating_system_name(
                           syscape::target_operating_system()),
                       "cmsis-rtos") == 0);
    return 0;
}
