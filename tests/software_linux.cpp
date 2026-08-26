#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/detail/software/linux.hpp>
#include <syscape/software.hpp>

namespace {

void test_proc_modules_parsing() {
    const std::string sample =
        "ext4 983040 1 - Live 0x0000000000000000\n"
        "crc32c_generic 12288 0 - Live 0x0000000000000000\n"
        "loading_mod 4096 0 - Loading 0x0000000000000000\n"
        "unloading_mod 8192 0 - Unloading 0x0000000000000000\n"
        "# Comment line\n"
        "\n";

    std::vector<syscape::detail::software_common::driver_record> records;
    assert(syscape::detail::software_backend::linux_impl::parse_proc_modules(sample, records));
    assert(records.size() == 4);

    assert(records[0].name == "ext4");
    assert(records[0].size_bytes.has_value() && *records[0].size_bytes == 983040);
    assert(records[0].use_count.has_value() && *records[0].use_count == 1);
    assert(records[0].state == syscape::detail::software_common::driver_state::running);

    assert(records[1].name == "crc32c_generic");
    assert(records[1].size_bytes.has_value() && *records[1].size_bytes == 12288);
    assert(records[1].use_count.has_value() && *records[1].use_count == 0);
    assert(records[1].state == syscape::detail::software_common::driver_state::running);

    assert(records[2].name == "loading_mod");
    assert(records[2].state == syscape::detail::software_common::driver_state::unknown);

    assert(records[3].name == "unloading_mod");
    assert(records[3].state == syscape::detail::software_common::driver_state::unloading);

    std::vector<syscape::detail::software_common::driver_record> malformed_records;
    assert(!syscape::detail::software_backend::linux_impl::parse_proc_modules(
        "invalid_line\n", malformed_records));
    assert(!syscape::detail::software_backend::linux_impl::parse_proc_modules(
        "bad_size nope 1 - Live 0x0\n", malformed_records));
    assert(!syscape::detail::software_backend::linux_impl::parse_proc_modules(
        "bad_count 1 nope - Live 0x0\n", malformed_records));
    assert(!syscape::detail::software_backend::linux_impl::parse_proc_modules(
        "truncated 1 0\n", malformed_records));
}

void test_systemd_service_parsing() {
    const std::string sample =
        "[Unit]\n"
        "Description=OpenBSD Secure Shell server\n"
        "Documentation=man:sshd(8) man:sshd_config(5)\n"
        "After=network.target auditd.service\n"
        "\n"
        "[Service]\n"
        "ExecStart=/usr/sbin/sshd -D $SSHD_OPTS\n"
        "ExecReload=/bin/kill -HUP $MAINPID\n"
        "KillMode=process\n"
        "Restart=on-failure\n"
        "\n"
        "[Install]\n"
        "WantedBy=multi-user.target\n"
        "Alias=sshd.service\n";

    syscape::detail::software_common::service_record rec;
    assert(syscape::detail::software_backend::linux_impl::parse_systemd_service_file(sample, "ssh.service", rec));

    assert(rec.name == "ssh.service");
    assert(rec.description.has_value() && *rec.description == "OpenBSD Secure Shell server");
    assert(rec.display_name.has_value() && *rec.display_name == "OpenBSD Secure Shell server");
    assert(rec.executable_path.has_value() && *rec.executable_path == "/usr/sbin/sshd -D $SSHD_OPTS");
    assert(rec.startup_type == syscape::detail::software_common::service_startup::manual);

    syscape::detail::software_common::service_record static_rec;
    assert(syscape::detail::software_backend::linux_impl::parse_systemd_service_file(
        "[Service]\nExecStart=/usr/bin/static-service\n",
        "static.service",
        static_rec));
    assert(static_rec.startup_type == syscape::detail::software_common::service_startup::unknown);
}

void test_pacman_desc_parsing() {
    const std::string sample =
        "%NAME%\n"
        "zlib\n"
        "\n"
        "%VERSION%\n"
        "1:1.3.1-1\n"
        "\n"
        "%BASE%\n"
        "zlib\n"
        "\n"
        "%DESC%\n"
        "Compression library implementing the deflate compression method found in gzip and pkzip\n"
        "\n"
        "%URL%\n"
        "https://www.zlib.net/\n"
        "\n"
        "%ARCH%\n"
        "x86_64\n"
        "\n"
        "%BUILDDATE%\n"
        "1705917336\n"
        "\n"
        "%PACKAGER%\n"
        "Arch Linux Release Team\n";

    syscape::detail::software_common::package_record rec;
    assert(syscape::detail::software_backend::linux_impl::parse_pacman_desc(sample, rec));
    assert(rec.name == "zlib");
    assert(rec.version.has_value() && *rec.version == "1:1.3.1-1");
    assert(rec.description.has_value() && rec.description->find("Compression library") != std::string::npos);
    assert(rec.architecture.has_value() && *rec.architecture == "x86_64");
    assert(rec.publisher.has_value() && *rec.publisher == "Arch Linux Release Team");
    assert(rec.format == syscape::detail::software_common::package_format::pacman);
}

void test_dpkg_status_parsing() {
    const std::string sample =
        "Package: curl\n"
        "Status: install ok installed\n"
        "Priority: optional\n"
        "Section: web\n"
        "Installed-Size: 450\n"
        "Maintainer: Ubuntu Developers <ubuntu-devel-discuss@lists.ubuntu.com>\n"
        "Architecture: amd64\n"
        "Version: 7.81.0-1ubuntu1.16\n"
        "Description: command line tool for transferring data with URL syntax\n"
        " A command line tool for transferring data with URL syntax\n"
        "\n"
        "Package: removed-pkg\n"
        "Status: deinstall ok config-files\n"
        "Version: 1.0.0\n"
        "\n"
        "Package: half-installed-pkg\n"
        "Status: install ok half-installed\n"
        "Version: 2.0.0\n"
        "\n";

    std::vector<syscape::detail::software_common::package_record> records;
    assert(syscape::detail::software_backend::linux_impl::parse_dpkg_status(sample, records));
    assert(records.size() == 1);
    assert(records[0].name == "curl");
    assert(records[0].version.has_value() && *records[0].version == "7.81.0-1ubuntu1.16");
    assert(records[0].architecture.has_value() && *records[0].architecture == "amd64");
    assert(records[0].publisher.has_value() && records[0].publisher->find("Ubuntu Developers") != std::string::npos);
    assert(records[0].format == syscape::detail::software_common::package_format::dpkg);
}

void test_apk_installed_parsing() {
    const std::string sample =
        "C:Q1xyz\n"
        "P:busybox\n"
        "V:1.36.1-r15\n"
        "A:x86_64\n"
        "S:952408\n"
        "I:1048576\n"
        "T:Size optimized toolbox of many common UNIX utilities\n"
        "U:https://busybox.net/\n"
        "L:GPL-2.0-only\n"
        "m:Maintainer Name <maintainer@example.org>\n"
        "\n";

    std::vector<syscape::detail::software_common::package_record> records;
    assert(syscape::detail::software_backend::linux_impl::parse_apk_installed(sample, records));
    assert(records.size() == 1);
    assert(records[0].name == "busybox");
    assert(records[0].version.has_value() && *records[0].version == "1.36.1-r15");
    assert(records[0].architecture.has_value() && *records[0].architecture == "x86_64");
    assert(records[0].publisher.has_value() && records[0].publisher->find("Maintainer Name") != std::string::npos);
    assert(records[0].description.has_value() && records[0].description->find("Size optimized") != std::string::npos);
    assert(records[0].format == syscape::detail::software_common::package_format::apk);
}

void test_desktop_entry_parsing() {
    const std::string sample =
        "[Desktop Entry]\n"
        "Name=Firefox Web Browser\n"
        "Comment=Browse the World Wide Web\n"
        "Exec=firefox %u\n"
        "Terminal=false\n"
        "Type=Application\n"
        "Icon=firefox\n"
        "Categories=Network;WebBrowser;\n";

    syscape::detail::software_common::package_record rec;
    assert(syscape::detail::software_backend::linux_impl::parse_desktop_entry(
        sample, "/usr/share/applications/firefox.desktop", rec));
    assert(rec.name == "Firefox Web Browser");
    assert(rec.description.has_value() && *rec.description == "Browse the World Wide Web");
    assert(!rec.install_location.has_value());
    assert(rec.format == syscape::detail::software_common::package_format::desktop_entry);
}

void test_live_drivers() {
    const auto drivers = syscape::software::loaded_drivers();
    if (!drivers) {
        return;
    }

    for (std::size_t i = 1; i < drivers->size(); ++i) {
        assert((*drivers)[i - 1].name <= (*drivers)[i].name);
    }

    if (!drivers->empty()) {
        const auto& first = (*drivers)[0];
        assert(!first.name.empty());
        const auto lookup = syscape::software::find_driver(first.name);
        assert(lookup);
        assert(lookup->name == first.name);
        assert(lookup->size_bytes == first.size_bytes);
    }

    const auto invalid = syscape::software::find_driver("");
    assert(!invalid);
    assert(invalid.error() == syscape::errc::invalid_argument);

    const auto missing = syscape::software::find_driver("__non_existent_module_xyz_123__");
    assert(!missing);
    assert(missing.error() == syscape::errc::not_found);
}

void test_live_services() {
    const auto svcs = syscape::software::services();
    if (!svcs) {
        return;
    }

    for (std::size_t i = 1; i < svcs->size(); ++i) {
        assert((*svcs)[i - 1].name <= (*svcs)[i].name);
    }

    if (!svcs->empty()) {
        const auto& first = (*svcs)[0];
        assert(!first.name.empty());
        const auto lookup = syscape::software::find_service(first.name);
        assert(lookup);
        assert(lookup->name == first.name);
    }

    const auto invalid = syscape::software::find_service("");
    assert(!invalid);
    assert(invalid.error() == syscape::errc::invalid_argument);

    const auto missing = syscape::software::find_service("__non_existent_service_xyz_123__");
    assert(!missing);
    assert(missing.error() == syscape::errc::not_found);
}

void test_live_packages() {
    const auto pkgs = syscape::software::installed_packages();
    if (!pkgs) {
        return;
    }

    if (!pkgs->empty()) {
        const auto& first = (*pkgs)[0];
        assert(!first.name.empty());
        const auto lookup = syscape::software::find_package(first.name);
        assert(lookup);
        assert(lookup->name == first.name);
    }

    const auto invalid = syscape::software::find_package("");
    assert(!invalid);
    assert(invalid.error() == syscape::errc::invalid_argument);

    const auto missing = syscape::software::find_package("__non_existent_pkg_xyz_123__");
    assert(!missing);
    assert(missing.error() == syscape::errc::not_found);
}

} // namespace

int main() {
    test_proc_modules_parsing();
    test_systemd_service_parsing();
    test_pacman_desc_parsing();
    test_dpkg_status_parsing();
    test_apk_installed_parsing();
    test_desktop_entry_parsing();

    test_live_drivers();
    test_live_services();
    test_live_packages();

    std::cout << "All software Linux tests passed.\n";
    return 0;
}
