#include <cassert>
#include <cstring>

#include <p4.h>

#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::pikeos,
              "Including <p4.h> must select pikeos");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "Including <p4.h> must select the rtos execution environment");

int main() {
    assert(std::strcmp(syscape::operating_system_name(
                           syscape::target_operating_system()),
                       "pikeos") == 0);
    return 0;
}
