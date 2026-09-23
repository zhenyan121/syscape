#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::openvms,
              "OpenVMS must select openvms operating system");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::hosted,
              "OpenVMS must select hosted execution environment");

int main() {
    return 0;
}
