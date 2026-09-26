#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::rtthread,
              "RT-Thread must select rtthread");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "RT-Thread must select the rtos execution environment");

int main() {
    return 0;
}
