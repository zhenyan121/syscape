#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::threadx,
              "THREADX must select ThreadX");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "ThreadX must select the rtos execution environment");

int main() {
    return 0;
}
