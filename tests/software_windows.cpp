#include <cassert>
#include <iostream>
#include <syscape/software.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_software_backend() {
#if defined(_WIN32)
    using namespace syscape::software;
    using namespace syscape::detail::software_backend::windows_impl;

    expect(!is_install_pending_cbs_state(0x05), "CBS uninstall-pending state must not be install-pending");
    expect(!is_install_pending_cbs_state(0x30), "CBS staging state must not be pending");
    expect(!is_install_pending_cbs_state(0x40), "CBS staged state must not be pending");
    expect(is_install_pending_cbs_state(0x60), "CBS install-pending state must be pending");

    expect(map_service_state(SERVICE_RUNNING) == service_state::running, "SERVICE_RUNNING state mapping");
    expect(map_service_state(SERVICE_STOPPED) == service_state::stopped, "SERVICE_STOPPED state mapping");
    expect(map_service_state(SERVICE_PAUSED) == service_state::paused, "SERVICE_PAUSED state mapping");
    expect(map_service_state(SERVICE_START_PENDING) == service_state::starting, "SERVICE_START_PENDING state mapping");
    expect(map_service_state(SERVICE_STOP_PENDING) == service_state::stopping, "SERVICE_STOP_PENDING state mapping");

    expect(map_service_startup(SERVICE_AUTO_START) == service_startup::automatic, "SERVICE_AUTO_START startup mapping");
    expect(map_service_startup(SERVICE_DEMAND_START) == service_startup::manual, "SERVICE_DEMAND_START startup mapping");
    expect(map_service_startup(SERVICE_DISABLED) == service_startup::disabled, "SERVICE_DISABLED startup mapping");

    const auto unchanged = expand_environment_string(L"C:\\Program Files\\Example");
    expect(unchanged && *unchanged == L"C:\\Program Files\\Example", "plain registry strings must remain unchanged");

    const auto expanded = expand_environment_string(L"%SystemRoot%\\System32");
    expect(expanded && expanded->find(L'%') == std::wstring::npos, "environment variables must be expanded");

    const auto svcs = services();
    if (svcs) {
        for (std::size_t i = 1; i < svcs->size(); ++i) {
            expect((*svcs)[i - 1].name <= (*svcs)[i].name, "services must be sorted");
        }
    }

    const auto drvs = loaded_drivers();
    if (drvs) {
        for (std::size_t i = 1; i < drvs->size(); ++i) {
            expect((*drvs)[i - 1].name <= (*drvs)[i].name, "drivers must be sorted");
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
    test_windows_software_backend();
    return failures;
}
