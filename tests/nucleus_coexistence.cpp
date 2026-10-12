#include <cassert>
#include <syscape/execution_environment.hpp>

#if defined(SYSCAPE_TARGET_NUCLEUS)
#error                                                                         \
    "SYSCAPE_TARGET_NUCLEUS must not be defined when another RTOS target is active"
#endif

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::freertos,
              "FreeRTOS must take precedence over Nucleus detection");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "Target execution environment must be rtos");

int main() {
    return 0;
}
