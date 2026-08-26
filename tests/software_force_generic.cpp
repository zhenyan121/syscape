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

    return 0;
}
