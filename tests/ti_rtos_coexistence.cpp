#include <cassert>
#include <syscape/execution_environment.hpp>

#if defined(SYSCAPE_TARGET_TI_RTOS)
#error                                                                         \
    "SYSCAPE_TARGET_TI_RTOS must not be defined when another RTOS target is active"
#endif

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::freertos,
              "FreeRTOS must take precedence over TI-RTOS detection");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "Target execution environment must be rtos");

int main() {
    return 0;
}
