#include <iostream>

#include <syscape/audio.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_audio_queries() {
    const auto list = syscape::audio::devices();
    expect(list.has_value() || list.error() == syscape::errc::not_supported ||
               list.error() == syscape::errc::permission_denied,
           "audio devices query must succeed or report expected error");
}

} // namespace

int main() {
    test_audio_queries();
    return failures == 0 ? 0 : 1;
}
