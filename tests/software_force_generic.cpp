#include <cassert>
#include <syscape/software.hpp>

int main() {
    const auto svcs = syscape::software::services();
    assert(!svcs);
    assert(svcs.error() == syscape::errc::not_supported);

    const auto svc = syscape::software::find_service("ssh");
    assert(!svc);
    assert(svc.error() == syscape::errc::not_supported);

    const auto drvs = syscape::software::loaded_drivers();
    assert(!drvs);
    assert(drvs.error() == syscape::errc::not_supported);

    const auto drv = syscape::software::find_driver("ext4");
    assert(!drv);
    assert(drv.error() == syscape::errc::not_supported);

    const auto pkgs = syscape::software::installed_packages();
    assert(!pkgs);
    assert(pkgs.error() == syscape::errc::not_supported);

    const auto pkg = syscape::software::find_package("git");
    assert(!pkg);
    assert(pkg.error() == syscape::errc::not_supported);

    const auto upds = syscape::software::system_updates();
    assert(!upds);
    assert(upds.error() == syscape::errc::not_supported);

    const auto runtimes = syscape::software::installed_runtimes();
    assert(!runtimes);
    assert(runtimes.error() == syscape::errc::not_supported);

    syscape::detail::software_common::update_record invalid_update;
    invalid_update.identifier = std::string("bad\xFF", 4);
    invalid_update.title = "Bad update";
    const auto public_update = syscape::detail::software_public::make_public_update(
        std::move(invalid_update));
    assert(!public_update);
    assert(public_update.error() == syscape::errc::invalid_encoding);

    syscape::detail::software_common::runtime_record invalid_runtime;
    invalid_runtime.name = "Runtime";
    invalid_runtime.version = "1.0";
    invalid_runtime.installation_path = std::string("/bad/\xFF", 6);
    const auto public_runtime = syscape::detail::software_public::make_public_runtime(
        std::move(invalid_runtime));
    assert(!public_runtime);
    assert(public_runtime.error() == syscape::errc::invalid_encoding);

    return 0;
}
