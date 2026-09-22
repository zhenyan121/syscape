#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::riot,
              "RIOT OS must select riot");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "RIOT OS must select the rtos execution environment");

int main() {
    return 0;
}
