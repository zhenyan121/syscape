#include <cassert>
#include <syscape/execution_environment.hpp>

#if defined(SYSCAPE_TARGET_CMSIS_RTOS)
#error                                                                         \
    "SYSCAPE_TARGET_CMSIS_RTOS must not be defined when another RTOS target is active"
#endif

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::freertos,
              "FreeRTOS must take precedence over CMSIS-RTOS wrapper");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "Target execution environment must be rtos");

int main() {
    return 0;
}
