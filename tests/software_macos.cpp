#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <syscape/software.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_macos_software_backend() {
#if defined(__APPLE__) && defined(__MACH__)
    using namespace syscape::software;
    using namespace syscape::detail::software_backend::macos_impl;

    std::string go_version;
    expect(parse_go_version_file(
               "go1.27.0\ntime 2026-08-18T21:24:23Z\n", go_version) &&
               go_version == "1.27.0",
           "Go VERSION parsing must ignore metadata lines");

    std::unordered_set<std::string> seen_updates;
    std::vector<syscape::detail::software_common::update_record> merged_updates;
    syscape::detail::software_common::update_record path_update;
    path_update.identifier = "product-1";
    path_update.title = "product-1";
    merge_update_record(std::move(path_update), seen_updates, merged_updates);
    syscape::detail::software_common::update_record metadata_update;
    metadata_update.identifier = "product-1";
    metadata_update.title = "Product One";
    metadata_update.version = "1.2.3";
    metadata_update.requires_reboot = true;
    merge_update_record(std::move(metadata_update), seen_updates, merged_updates);
    expect(merged_updates.size() == 1 &&
               merged_updates[0].title == "Product One" &&
               merged_updates[0].version == std::optional<std::string>("1.2.3") &&
               merged_updates[0].requires_reboot,
           "duplicate macOS update metadata must be merged");

    const auto drvs = loaded_drivers();
    expect(!drvs && drvs.error() == syscape::errc::not_supported, "macOS loaded_drivers must report not_supported");

    const auto svcs = services();
    if (svcs) {
        for (std::size_t i = 1; i < svcs->size(); ++i) {
            expect((*svcs)[i - 1].name <= (*svcs)[i].name, "services must be sorted");
        }
    }

    const auto pkgs = installed_packages();
    if (pkgs) {
        for (std::size_t i = 1; i < pkgs->size(); ++i) {
            expect((*pkgs)[i - 1].name <= (*pkgs)[i].name, "packages must be sorted");
        }
    }

    const auto upds = system_updates();
    if (upds) {
        for (std::size_t i = 1; i < upds->size(); ++i) {
            expect((*upds)[i - 1].identifier <= (*upds)[i].identifier, "updates must be sorted");
        }
    }

    const auto runtimes = installed_runtimes();
    if (runtimes) {
        for (std::size_t i = 1; i < runtimes->size(); ++i) {
            expect(static_cast<int>((*runtimes)[i - 1].kind) <= static_cast<int>((*runtimes)[i].kind), "runtimes must be sorted");
        }
    }
#endif
}

} // namespace

int main() {
    test_macos_software_backend();
    return failures;
}
