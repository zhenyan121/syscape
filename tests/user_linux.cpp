#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

#include <syscape/detail/user/common.hpp>
#include <syscape/detail/user/linux.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/user.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct fake_lookup {
    ::passwd entry {};
    const char* name_value = nullptr;
    const char* directory_value = nullptr;
    const char* shell_value = nullptr;
    std::size_t erange_until_size = 0U;
    int failure_code = 0;
    bool found = true;

    int operator()(::passwd& target, char* /*buffer*/, std::size_t size,
                   ::passwd** result) {
        if (failure_code != 0) { return failure_code; }
        if (size < erange_until_size) { return ERANGE; }
        if (!found) {
            *result = nullptr;
            return 0;
        }

        target = ::passwd {};
        target.pw_name = const_cast<char*>(name_value);
        target.pw_dir = const_cast<char*>(directory_value);
        target.pw_shell = const_cast<char*>(shell_value);
        *result = &target;
        return 0;
    }
};

void test_passwd_lookup_success() {
    fake_lookup operation;
    operation.name_value = "alice";
    operation.directory_value = "/home/alice";
    operation.shell_value = "";

    const auto fields =
        syscape::detail::user_backend::lookup_passwd_with_growth(operation);
    expect(fields && fields->name == "alice" &&
               fields->directory == "/home/alice" && fields->shell.empty(),
           "A passwd entry must expose its fields verbatim");

    fake_lookup missing;
    missing.found = false;
    const auto absent =
        syscape::detail::user_backend::lookup_passwd_with_growth(missing);
    expect(!absent &&
               absent.error() ==
                   syscape::make_error_code(syscape::errc::not_found),
           "An absent passwd entry must report not_found");
}

void test_identifier_narrowing() {
    const auto maximum = syscape::detail::user_backend::narrow_identifier(
        std::numeric_limits<std::uint32_t>::max());
    expect(maximum &&
               *maximum == std::numeric_limits<std::uint32_t>::max(),
           "The largest portable identifier must remain representable");

    const std::uint64_t oversized =
        static_cast<std::uint64_t>(
            std::numeric_limits<std::uint32_t>::max()) +
        1U;
    const auto rejected =
        syscape::detail::user_backend::narrow_identifier(oversized);
    expect(!rejected && rejected.error() == syscape::errc::value_too_large,
           "An oversized native identifier must report value_too_large");
}

void test_passwd_buffer_growth() {
    fake_lookup growing;
    growing.name_value = "bob";
    growing.directory_value = "/home/bob";
    growing.shell_value = "/bin/sh";
    growing.erange_until_size = 4096U;

    const auto grown =
        syscape::detail::user_backend::lookup_passwd_with_growth(growing);
    expect(grown && grown->name == "bob" &&
               grown->directory == "/home/bob" &&
               grown->shell == "/bin/sh",
           "Passwd lookups must retry into larger buffers on ERANGE");
}

void test_passwd_failures() {
    fake_lookup interrupted;
    interrupted.failure_code = EIO;
    const auto failed =
        syscape::detail::user_backend::lookup_passwd_with_growth(interrupted);
    expect(!failed && failed.error() ==
                          std::error_code(EIO, std::generic_category()),
           "Native passwd lookup failures must preserve their error code");

    fake_lookup oversized;
    oversized.failure_code = ERANGE;
    const auto exhausted =
        syscape::detail::user_backend::lookup_passwd_with_growth(oversized);
    expect(!exhausted && exhausted.error() ==
                             std::error_code(ERANGE, std::generic_category()),
           "Permanently undersized passwd data must fail instead of looping");

    fake_lookup unnamed;
    unnamed.shell_value = "/bin/sh";
    const auto nameless =
        syscape::detail::user_backend::lookup_passwd_with_growth(unnamed);
    expect(nameless && nameless->name.empty() &&
               nameless->shell == "/bin/sh",
           "Fields must be copied verbatim even when a field is unusable");

    syscape::detail::user_backend::passwd_fields unusable_name;
    unusable_name.name.clear();
    unusable_name.directory = "/home/carol";
    unusable_name.shell = "/bin/sh";
    const auto rejected_name =
        syscape::detail::user_backend::extract_name(unusable_name);
    expect(!rejected_name &&
               rejected_name.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "A passwd entry without a usable name is malformed platform data");
    const auto independent_shell =
        syscape::detail::user_backend::extract_shell(unusable_name);
    expect(independent_shell && *independent_shell == "/bin/sh",
           "A bad name must not invalidate the recorded shell");

    syscape::detail::user_backend::passwd_fields relative_home;
    relative_home.name = "carol";
    relative_home.directory = "relative/carol";
    relative_home.shell = "/bin/sh";
    const auto rejected_directory =
        syscape::detail::user_backend::extract_home_directory(relative_home);
    expect(!rejected_directory &&
               rejected_directory.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "A relative recorded home directory is malformed platform data");
    const auto independent_name =
        syscape::detail::user_backend::extract_name(relative_home);
    expect(independent_name && *independent_name == "carol",
           "A relative home directory must not invalidate the login name");
}

void test_text_boundaries() {
    std::string invalid(1U, static_cast<char>(0xff));

    const auto invalid_name = syscape::detail::user_common::validate_utf8_name(
        syscape::result<std::string>(invalid));
    expect(!invalid_name &&
               invalid_name.error() == syscape::errc::invalid_encoding,
           "Non-UTF-8 names must fail at the public boundary");

    const auto empty_name = syscape::detail::user_common::validate_utf8_name(
        syscape::result<std::string>(""));
    expect(!empty_name &&
               empty_name.error() == syscape::errc::malformed_data,
           "Empty names must be rejected as malformed platform data");

    const auto invalid_home = syscape::detail::user_common::validate_utf8_path(
        syscape::result<std::string>(std::string("/tmp/\xffx")));
    expect(!invalid_home &&
               invalid_home.error() == syscape::errc::invalid_encoding,
           "Non-UTF-8 paths must fail at the public boundary");

    const auto empty_shell = syscape::detail::user_common::validate_utf8_shell(
        syscape::result<std::string>(""));
    expect(empty_shell && empty_shell->empty(),
           "An empty recorded shell is valid data at the public boundary");

    const auto invalid_shell =
        syscape::detail::user_common::validate_utf8_shell(
            syscape::result<std::string>(invalid));
    expect(!invalid_shell &&
               invalid_shell.error() == syscape::errc::invalid_encoding,
           "Non-UTF-8 shells must fail at the public boundary");
}

void test_runtime_queries() {
    const auto real_user = syscape::user::real_user_id();
    expect(real_user && *real_user == static_cast<std::uint32_t>(::getuid()),
           "Linux must report the real user ID from getuid()");

    const auto effective_user = syscape::user::effective_user_id();
    expect(effective_user &&
               *effective_user == static_cast<std::uint32_t>(::geteuid()),
           "Linux must report the effective user ID from geteuid()");

    const auto real_group = syscape::user::real_group_id();
    expect(real_group &&
               *real_group == static_cast<std::uint32_t>(::getgid()),
           "Linux must report the real group ID from getgid()");

    const auto effective_group = syscape::user::effective_group_id();
    expect(effective_group &&
               *effective_group == static_cast<std::uint32_t>(::getegid()),
           "Linux must report the effective group ID from getegid()");

    // The reference lookup mirrors the backend's growth behavior and treats
    // an absent entry as a legitimate environment, for example in a
    // container whose effective user has no passwd record.
    constexpr std::size_t maximum_reference_size = 1024U * 1024U;
    std::vector<char> reference_buffer(4096U);
    ::passwd reference {};
    ::passwd* reference_pointer = nullptr;
    int outcome = 0;
    for (;;) {
        outcome = ::getpwuid_r(::geteuid(), &reference,
                               reference_buffer.data(),
                               reference_buffer.size(), &reference_pointer);
        if (outcome == ERANGE &&
            reference_buffer.size() < maximum_reference_size) {
            reference_buffer.resize(reference_buffer.size() * 2U);
            continue;
        }
        break;
    }
    expect(outcome == 0, "The passwd reference lookup must not fail natively");
    const bool reference_exists = outcome == 0 && reference_pointer != nullptr;

    const auto name = syscape::user::user_name();
    const auto home = syscape::user::home_directory();
    const auto shell = syscape::user::shell();

    if (!reference_exists) {
        const std::error_code not_found =
            syscape::make_error_code(syscape::errc::not_found);
        expect(!name && name.error() == not_found &&
                   !home && home.error() == not_found &&
                   !shell && shell.error() == not_found,
               "Without a passwd entry every textual query must report "
               "not_found instead of fabricating data");
        return;
    }

    const std::string reference_name =
        reference_pointer->pw_name != nullptr
            ? std::string(reference_pointer->pw_name)
            : std::string();
    if (reference_name.empty()) {
        expect(!name && name.error() == syscape::errc::malformed_data,
               "An empty recorded name must report malformed_data");
    } else if (!syscape::detail::is_valid_utf8(reference_name)) {
        expect(!name && name.error() == syscape::errc::invalid_encoding,
               "A non-UTF-8 recorded name must report invalid_encoding");
    } else {
        expect(name && *name == reference_name,
               "Linux must report the effective user's login name from the "
               "passwd database");
    }

    const std::string reference_home =
        reference_pointer->pw_dir != nullptr
            ? std::string(reference_pointer->pw_dir)
            : std::string();
    if (reference_home.empty() || reference_home.front() != '/') {
        expect(!home && home.error() == syscape::errc::malformed_data,
               "An empty or relative recorded home must report "
               "malformed_data");
    } else if (!syscape::detail::is_valid_utf8(reference_home)) {
        expect(!home && home.error() == syscape::errc::invalid_encoding,
               "A non-UTF-8 recorded home must report invalid_encoding");
    } else {
        expect(home && *home == reference_home,
               "Linux must report the effective user's absolute home "
               "directory from the passwd database");
    }

    const std::string reference_shell =
        reference_pointer->pw_shell != nullptr
            ? std::string(reference_pointer->pw_shell)
            : std::string();
    if (syscape::detail::is_valid_utf8(reference_shell)) {
        expect(shell && *shell == reference_shell,
               "Linux must report the recorded UTF-8 shell verbatim");
    } else {
        expect(!shell && shell.error() == syscape::errc::invalid_encoding,
               "A non-UTF-8 recorded shell must report invalid_encoding");
    }
}

} // namespace

int main() {
    test_passwd_lookup_success();
    test_identifier_narrowing();
    test_passwd_buffer_growth();
    test_passwd_failures();
    test_text_boundaries();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
