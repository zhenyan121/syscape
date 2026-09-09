#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::wasi,
              "__wasi__ must select WASI");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::sandboxed,
              "WASI must select the sandboxed execution environment");

int main() {
    return 0;
}
