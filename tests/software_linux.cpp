#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

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

void test_reboot_required_parsing() {
    const std::string sample =
        "linux-image-6.8.0-40-generic\n"
        "linux-base\n"
        "# Comment\n"
        "dbus\n"
        "\n";

    std::vector<syscape::detail::software_common::update_record> records;
    assert(syscape::detail::software_backend::linux_impl::parse_reboot_required_pkgs(sample, records));
    assert(records.size() == 3);
    assert(records[0].identifier == "linux-image-6.8.0-40-generic");
    assert(records[0].requires_reboot == true);
    assert(records[0].classification == syscape::detail::software_common::update_classification::unknown);
    assert(records[0].severity == syscape::detail::software_common::update_severity::unknown);
    assert(records[1].identifier == "linux-base");
    assert(records[2].identifier == "dbus");
}

void test_checkupdates_parsing() {
    const std::string sample = "linux 6.10.1.arch1-1 -> 6.10.2.arch1-1\n"
                               "openssl 3.3.1-1 -> 3.3.1-2\n"
                               "epoch-package 2:1.0-1 -> 2:1.1-1\n";
    std::vector<syscape::detail::software_common::update_record> records;
    assert(syscape::detail::software_backend::linux_impl::
               parse_checkupdates_output(sample, records));
    assert(records.size() == 3);
    assert(records[0].identifier == "linux");
    assert(records[0].version.has_value() &&
           *records[0].version == "6.10.2.arch1-1");
    assert(records[0].requires_reboot);
    assert(records[1].identifier == "openssl");
    assert(records[1].version.has_value() && *records[1].version == "3.3.1-2");
    assert(!records[1].requires_reboot);
    assert(records[2].version.has_value() && *records[2].version == "2:1.1-1");

    records.clear();
    assert(!syscape::detail::software_backend::linux_impl::
               parse_checkupdates_output(
                   "valid 1-1 -> 1-2\nmissing-version-separator\n", records));
    assert(records.empty());
}

void test_packagekit_prepared_update_parsing() {
    // 1. Official PackageKit GKeyFile with comma-separated prepared_ids
    const std::string official_comma_sample =
        "[update]\n"
        "prepared_ids=nano;8.0-1;x86_64;fedora,firefox;128.0-1.fc40;x86_64;updates,linux;6.10.1;x86_64;updates,\n"
        "action=reboot\n";

    bool is_malformed = false;
    std::vector<syscape::detail::software_common::update_record> records;
    assert(syscape::detail::software_backend::linux_impl::parse_packagekit_prepared_update(
        official_comma_sample, records, is_malformed));
    assert(!is_malformed);
    assert(records.size() == 3);
    assert(records[0].identifier == "nano");
    assert(records[0].version.has_value() && *records[0].version == "8.0-1");
    assert(!records[0].requires_reboot);

    assert(records[1].identifier == "firefox");
    assert(records[1].version.has_value() && *records[1].version == "128.0-1.fc40");
    assert(!records[1].requires_reboot);

    assert(records[2].identifier == "linux");
    assert(records[2].requires_reboot);

    // 2. Single package ID in prepared_ids without trailing comma
    const std::string single_sample =
        "[update]\n"
        "prepared_ids=systemd;256.4-1;x86_64;updates\n";

    records.clear();
    assert(syscape::detail::software_backend::linux_impl::parse_packagekit_prepared_update(
        single_sample, records, is_malformed));
    assert(!is_malformed);
    assert(records.size() == 1);
    assert(records[0].identifier == "systemd");
    assert(records[0].requires_reboot);

    // 3. PackageKit permits empty architecture and data fields, but retains
    // all three semicolon separators.
    const std::string optional_fields_sample =
        "[update]\n"
        "prepared_ids=portable-package;1.0;;\n";

    records.clear();
    assert(syscape::detail::software_backend::linux_impl::parse_packagekit_prepared_update(
        optional_fields_sample, records, is_malformed));
    assert(!is_malformed);
    assert(records.size() == 1);
    assert(records[0].identifier == "portable-package");
    assert(records[0].version.has_value() && *records[0].version == "1.0");

    // 4. Legacy line-oriented format
    const std::string legacy_sample =
        "nano;8.0-1;x86_64;fedora\n"
        "firefox;128.0-1.fc40;x86_64;updates\n";

    records.clear();
    assert(syscape::detail::software_backend::linux_impl::parse_packagekit_prepared_update(
        legacy_sample, records, is_malformed));
    assert(!is_malformed);
    assert(records.size() == 2);
    assert(records[0].identifier == "nano");
    assert(records[1].identifier == "firefox");

    // 5. Malformed case: Missing [update] section
    records.clear();
    is_malformed = false;
    const std::string wrong_section =
        "[not_update]\n"
        "prepared_ids=nano;8.0-1;x86_64;fedora\n";
    assert(!syscape::detail::software_backend::linux_impl::parse_packagekit_prepared_update(
        wrong_section, records, is_malformed));
    assert(is_malformed);
    assert(records.empty());

    // 6. Malformed case: Missing prepared_ids key in [update] section
    records.clear();
    is_malformed = false;
    const std::string missing_key =
        "[update]\n"
        "action=reboot\n";
    assert(!syscape::detail::software_backend::linux_impl::parse_packagekit_prepared_update(
        missing_key, records, is_malformed));
    assert(is_malformed);
    assert(records.empty());

    // 7. Malformed case: Empty package ID element
    records.clear();
    is_malformed = false;
    const std::string invalid_pkg_id =
        "[update]\n"
        "prepared_ids=;\n";
    assert(!syscape::detail::software_backend::linux_impl::parse_packagekit_prepared_update(
        invalid_pkg_id, records, is_malformed));
    assert(is_malformed);
    assert(records.empty());

    // 8. Malformed cases: empty files and package IDs without exactly four fields
    const std::string malformed_package_ids[] = {
        "",
        "[update]\nprepared_ids=nano\n",
        "[update]\nprepared_ids=nano;8.0-1\n",
        "[update]\nprepared_ids=nano;;x86_64;updates\n",
        "[update]\nprepared_ids=nano;8.0-1;x86_64;updates;extra\n"
    };
    for (const auto& malformed : malformed_package_ids) {
        records.clear();
        is_malformed = false;
        assert(!syscape::detail::software_backend::linux_impl::parse_packagekit_prepared_update(
            malformed, records, is_malformed));
        assert(is_malformed);
        assert(records.empty());
    }
}

void test_executable_file_detection() {
    std::error_code error;
    const char* executable_paths[] = { "/bin/sh" };
    assert(syscape::detail::software_backend::linux_impl::has_executable_file(
        executable_paths, 1, error));
    assert(!error);

    const char* missing_paths[] = { "/syscape-test-path-that-does-not-exist" };
    assert(!syscape::detail::software_backend::linux_impl::has_executable_file(
        missing_paths, 1, error));
    assert(!error);

    const char* directory_paths[] = { "/" };
    assert(!syscape::detail::software_backend::linux_impl::has_executable_file(
        directory_paths, 1, error));
    assert(!error);

    char inaccessible_path[] = "/tmp/syscape-software-test-XXXXXX";
    const int fd = ::mkstemp(inaccessible_path);
    assert(fd >= 0);
    assert(::close(fd) == 0);
    assert(::chmod(inaccessible_path, 0) == 0);
    error.clear();
    const char* inaccessible_paths[] = { inaccessible_path };
    const bool inaccessible_is_executable =
        syscape::detail::software_backend::linux_impl::has_executable_file(
            inaccessible_paths, 1, error);
    const int unlink_status = ::unlink(inaccessible_path);
    assert(unlink_status == 0);
    assert(!inaccessible_is_executable);
    assert(error == std::errc::permission_denied);

    error.clear();
    assert(syscape::detail::software_backend::linux_impl::has_directory("/", error));
    assert(!error);
    assert(!syscape::detail::software_backend::linux_impl::has_directory("/bin/sh", error));
    assert(!error);
}

void test_rust_manifest_parsing() {
    const std::string manifest_sample =
        "manifest-version = \"2\"\n"
        "date = \"2024-08-06\"\n"
        "\n"
        "[pkg.cargo]\n"
        "version = \"0.81.0 (7e71089 2024-08-01)\"\n"
        "\n"
        "[pkg.rustc]\n"
        "version = \"1.80.1 (3f5370e35 2024-08-06)\"\n"
        "\n"
        "[pkg.rustc.target.x86_64-unknown-linux-gnu]\n"
        "available = true\n";

    std::string ver;
    assert(syscape::detail::software_backend::linux_impl::parse_rust_channel_manifest(manifest_sample, ver));
    assert(ver == "1.80.1");
}

void test_rust_toolchain_parsing() {
    const std::string manifest_sample =
        "[pkg.rustc]\nversion = \"1.97.1 (8bab26f4f 2026-07-14)\"\n";

    std::string ver;
    assert(syscape::detail::software_backend::linux_impl::parse_rust_toolchain_version(
        "stable-x86_64-unknown-linux-gnu", manifest_sample, ver));
    assert(ver == "1.97.1");

    std::string ver2;
    assert(syscape::detail::software_backend::linux_impl::parse_rust_toolchain_version(
        "1.79.0-x86_64-unknown-linux-gnu", "", ver2));
    assert(ver2 == "1.79.0");

    std::string ver3;
    assert(syscape::detail::software_backend::linux_impl::parse_rust_toolchain_version(
        "nightly-2024-05-01-x86_64-unknown-linux-gnu", "", ver3));
    assert(ver3 == "nightly-2024-05-01");

    // Must NOT fall back to fake "stable" or "beta"
    std::string ver4;
    assert(!syscape::detail::software_backend::linux_impl::parse_rust_toolchain_version(
        "stable-x86_64-unknown-linux-gnu", "", ver4));
    assert(ver4.empty());
}

void test_java_release_parsing() {
    const std::string sample =
        "IMPLEMENTOR=\"Arch Linux\"\n"
        "JAVA_RUNTIME_VERSION=\"21.0.12.1+1\"\n"
        "JAVA_VERSION=\"21.0.12.1\"\n"
        "OS_ARCH=\"x86_64\"\n"
        "OS_NAME=\"Linux\"\n";

    std::string ver, impl, arch;
    assert(syscape::detail::software_backend::linux_impl::parse_java_release_file(sample, ver, impl, arch));
    assert(ver == "21.0.12.1");
    assert(impl == "Arch Linux");
    assert(arch == "x86_64");
}

void test_go_version_parsing() {
    std::string ver;
    assert(syscape::detail::software_backend::linux_impl::parse_go_version_file("go1.22.5\n", ver));
    assert(ver == "1.22.5");

    std::string ver2;
    assert(syscape::detail::software_backend::linux_impl::parse_go_version_file("1.21.0", ver2));
    assert(ver2 == "1.21.0");

    std::string ver3;
    assert(syscape::detail::software_backend::linux_impl::parse_go_version_file(
        "go1.27.0\ntime 2026-08-18T21:24:23Z\n", ver3));
    assert(ver3 == "1.27.0");
}

void test_node_version_parsing() {
    const std::string sample =
        "#define NODE_MAJOR_VERSION 20\n"
        "#define NODE_MINOR_VERSION 11\n"
        "#define NODE_PATCH_VERSION 0\n";

    std::string ver;
    assert(syscape::detail::software_backend::linux_impl::parse_node_version_header(sample, ver));
    assert(ver == "20.11.0");
}

void test_live_system_updates() {
    const auto upds = syscape::software::system_updates();
    if (upds) {
        for (std::size_t i = 1; i < upds->size(); ++i) {
            assert((*upds)[i - 1].identifier <= (*upds)[i].identifier);
        }
    } else {
        assert(upds.error() == syscape::errc::not_supported ||
               upds.error() == syscape::errc::permission_denied ||
               upds.error() == syscape::errc::temporarily_unavailable);
    }
}

void test_live_installed_runtimes() {
    const auto runtimes = syscape::software::installed_runtimes();
    assert(runtimes);
    for (const auto& rt : *runtimes) {
        assert(!rt.name.empty());
        assert(!rt.version.empty());
        assert(!rt.installation_path.empty());
        assert(rt.version.find('\n') == std::string::npos);
        assert(syscape::detail::is_valid_utf8(rt.name));
        assert(syscape::detail::is_valid_utf8(rt.version));
        assert(syscape::detail::is_valid_utf8(rt.installation_path));
    }
    for (std::size_t i = 1; i < runtimes->size(); ++i) {
        assert(static_cast<int>((*runtimes)[i - 1].kind) <= static_cast<int>((*runtimes)[i].kind));
    }
}

} // namespace

int main() {
    test_proc_modules_parsing();
    test_systemd_service_parsing();
    test_pacman_desc_parsing();
    test_dpkg_status_parsing();
    test_apk_installed_parsing();
    test_desktop_entry_parsing();
    test_reboot_required_parsing();
    test_checkupdates_parsing();
    test_packagekit_prepared_update_parsing();
    test_executable_file_detection();
    test_rust_manifest_parsing();
    test_java_release_parsing();
    test_go_version_parsing();
    test_node_version_parsing();
    test_rust_toolchain_parsing();

    test_live_drivers();
    test_live_services();
    test_live_packages();
    test_live_system_updates();
    test_live_installed_runtimes();

    std::cout << "All software Linux tests passed.\n";
    return 0;
}
