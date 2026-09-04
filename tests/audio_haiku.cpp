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
    const auto d = syscape::audio::devices();
    expect(!d && d.error() == syscape::errc::not_supported,
           "devices query must report not_supported on Haiku");
}

} // namespace

int main() {
    test_audio_queries();
    return failures == 0 ? 0 : 1;
}
