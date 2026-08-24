#include <iostream>
#include <string>
#include <system_error>

#include <syscape/hardware.hpp>
#include <syscape/detail/hardware/macos.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_fact_interpretation() {
    namespace backend = syscape::detail::hardware_backend;

    const syscape::result<std::string> recorded =
        backend::interpret_text(true, std::string("Apple"));
    expect(recorded.has_value() && *recorded == "Apple",
           "A recorded identity fact passes through verbatim");

    const syscape::result<std::string> absent =
        backend::interpret_text(false, std::string("ignored"));
    expect(absent.error() == syscape::errc::not_found,
           "A platform-expert key the machine omits records absence");

    expect(backend::interpret_text(true, std::string()).error() ==
               syscape::errc::not_found,
           "An empty identity rendering records absence instead of "
           "presenting nothing as data");
}

/// Exercises every public query against the running system. The checks stay
/// tolerant about which facts this particular machine records, because the
/// interface documents per-key absence, but they require every outcome to be
/// explicit rather than fabricated.
template <typename Query>
void check_live(const char* label, const Query& query) {
    const auto value = query();
    if (value) { return; }
    const std::error_code error = value.error();
    if (error == syscape::errc::not_supported ||
        error == syscape::errc::not_found) {
        return;
    }
    std::cerr << "FAIL: " << label << " reported " << error.message()
              << '\n';
    ++failures;
}

} // namespace

int main() {
    test_fact_interpretation();

    check_live("system_manufacturer", syscape::hardware::system_manufacturer);
    check_live("system_product_name", syscape::hardware::system_product_name);
    check_live("system_product_version",
               syscape::hardware::system_product_version);
    check_live("motherboard_manufacturer",
               syscape::hardware::motherboard_manufacturer);
    check_live("motherboard_product_name",
               syscape::hardware::motherboard_product_name);
    check_live("motherboard_version", syscape::hardware::motherboard_version);
    check_live("firmware_vendor", syscape::hardware::firmware_vendor);
    check_live("firmware_version", syscape::hardware::firmware_version);
    check_live("firmware_release_date",
               syscape::hardware::firmware_release_date);
    check_live("chassis_form_factor",
               syscape::hardware::chassis_form_factor);
    check_live("hardware_uuid", syscape::hardware::hardware_uuid);

    return failures == 0 ? 0 : 1;
}
