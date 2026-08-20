#include <syscape/execution_environment.hpp>
#include <syscape/execution_environment.hpp>

int main() {
    return syscape::operating_system_name(syscape::target_operating_system()) ==
                   nullptr
               ? 1
               : 0;
}
