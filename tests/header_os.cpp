#include <syscape/os.hpp>
#include <syscape/os.hpp>

int main() {
    const syscape::result<std::chrono::milliseconds> value = syscape::os::uptime();
    return value && value->count() < 0 ? 1 : 0;
}
