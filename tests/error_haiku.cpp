#include <Errors.h>
#include <OS.h>

#include <iostream>

#include <syscape/detail/haiku/error.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_error_mapping() {
    expect(!syscape::detail::haiku_error::make_haiku_error(B_OK),
           "B_OK must not produce an error");
    expect(syscape::detail::haiku_error::make_haiku_error(B_NO_MEMORY) ==
               syscape::errc::resource_exhausted,
           "B_NO_MEMORY must map to resource_exhausted");
}

} // namespace

int main() {
    test_error_mapping();
    return failures == 0 ? 0 : 1;
}
