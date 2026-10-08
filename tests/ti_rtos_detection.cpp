#include <cassert>
#include <cstring>
#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::ti_rtos,
              "TI-RTOS must select ti_rtos");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "TI-RTOS must select the rtos execution environment");

#if defined(SYSCAPE_TARGET_SYSBIOS)
#if !defined(SYSCAPE_TARGET_SYSBIOS_STANDALONE)
#error                                                                         \
    "SYSCAPE_TARGET_SYSBIOS_STANDALONE must be defined when SYSCAPE_TARGET_SYSBIOS is active"
#endif
#endif

int main() {
    assert(std::strcmp(syscape::operating_system_name(
                           syscape::target_operating_system()),
                       "ti-rtos") == 0);
    return 0;
}
