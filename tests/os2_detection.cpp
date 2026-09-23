#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::os2,
              "OS/2 must select os2 operating system");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::hosted,
              "OS/2 must select hosted execution environment");

int main() {
    return 0;
}
