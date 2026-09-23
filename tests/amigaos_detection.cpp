#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::amigaos,
              "AmigaOS must select amigaos operating system");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "AmigaOS must select rtos execution environment");

int main() {
    return 0;
}
