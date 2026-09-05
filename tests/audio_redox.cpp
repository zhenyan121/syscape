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
    expect(!devs && devs.error() == syscape::errc::not_supported,
           "audio devices query must report not_supported on Redox OS");

    const auto count = syscape::audio::device_count();
    expect(!count && count.error() == syscape::errc::not_supported,
           "device_count must report not_supported on Redox OS");
}

} // namespace

int main() {
    test_audio_queries();
    return failures == 0 ? 0 : 1;
}
