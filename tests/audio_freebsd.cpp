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
    const auto devs = syscape::audio::devices();
    expect(devs.has_value(), "audio devices query must succeed");
}

} // namespace

int main() {
    test_audio_queries();
    return failures == 0 ? 0 : 1;
}
