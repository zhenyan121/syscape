#ifndef SYSCAPE_DETAIL_LOCALE_PREFERENCES_LINUX_HPP
#define SYSCAPE_DETAIL_LOCALE_PREFERENCES_LINUX_HPP

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#include <syscape/detail/linux/file.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_backend {

/// The documented default system time-zone directory used when TZDIR is not
/// configured.
///
/// See the kernel userspace documentation and the tzset(3) description of
/// the system time-zone directory.
constexpr const char* default_zoneinfo_root = "/usr/share/zoneinfo";

/// The documented system-wide local time-zone configuration link whose
/// symlink target names the configured time-zone identifier.
///
/// See localtime(5): the file "should be an absolute or relative symbolic
/// link pointing to /usr/share/zoneinfo/, followed by a timezone
/// identifier", and the identifier "is extracted from the symlink target".
constexpr const char* localtime_link = "/etc/localtime";

/// Linux exposes no documented in-process source for a system-level list of
/// preferred languages.
///
/// Message-translation environment overrides such as LANGUAGE describe
/// process state rather than platform configuration, and decomposing locale
/// strings would fabricate structure the platform does not record, so the
/// query honestly reports the capability as unsupported instead.
inline result<std::vector<std::string>> preferred_languages() {
    return fail(errc::not_supported);
}

/// Linux exposes no documented in-process source for a country or region
/// code separate from the locale identifier itself, which current_locale()
/// already reports verbatim.
inline result<std::string> country_region_code() {
    return fail(errc::not_supported);
}

/// Resolves a path lexically against an absolute base directory.
inline std::string normalize_absolute_path(std::string_view path,
                                           std::string_view base) {
    std::vector<std::string_view> components;
    const auto append_components = [&components](std::string_view value) {
        std::size_t begin = 0U;
        for (;;) {
            const std::size_t end = value.find('/', begin);
            const std::string_view part =
                end == std::string_view::npos
                    ? value.substr(begin)
                    : value.substr(begin, end - begin);
            if (!part.empty() && part != ".") {
                if (part == "..") {
                    if (!components.empty()) { components.pop_back(); }
                } else {
                    components.push_back(part);
                }
            }
            if (end == std::string_view::npos) { break; }
            begin = end + 1U;
        }
    };

    if (path.empty() || path.front() != '/') { append_components(base); }
    append_components(path);

    std::string normalized;
    if (components.empty()) { return std::string("/"); }
    for (const std::string_view part : components) {
        normalized.push_back('/');
        normalized.append(part);
    }
    return normalized;
}

/// Returns the current working directory for resolving a relative TZDIR.
inline result<std::string> current_working_directory() {
    constexpr std::size_t maximum_size = 8192U;
    std::string buffer(256U, '\0');
    for (;;) {
        errno = 0;
        if (::getcwd(&buffer[0], buffer.size()) != nullptr) {
            buffer.resize(std::strlen(buffer.c_str()));
            return buffer;
        }
        if (errno != ERANGE) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (buffer.size() >= maximum_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() * 2U);
    }
}

/// Resolves a configured zoneinfo root to one normalized absolute path.
inline result<std::string> absolute_zoneinfo_root(
    std::string_view zoneinfo_root) {
    if (zoneinfo_root.empty()) { return fail(errc::not_found); }
    if (zoneinfo_root.front() == '/') {
        return normalize_absolute_path(zoneinfo_root, "/");
    }
    const result<std::string> working_directory = current_working_directory();
    if (!working_directory) { return fail(working_directory.error()); }
    return normalize_absolute_path(zoneinfo_root, *working_directory);
}

/// Extracts the time-zone identifier recorded below a zoneinfo root from one
/// symbolic-link target.
///
/// The target is resolved lexically against its containing directory so that
/// "." and ".." renderings cannot escape the root, and the identifier is the
/// remaining relative path below that root verbatim. A target that resolves
/// to the root itself or anywhere outside the root records no identifier,
/// which is reported as not_found rather than guessed from a basename.
inline result<std::string> zone_identifier_from_target(
    std::string_view target, std::string_view zoneinfo_root,
    std::string_view target_base = "/etc") {
    const result<std::string> normalized_root =
        absolute_zoneinfo_root(zoneinfo_root);
    if (!normalized_root) { return fail(normalized_root.error()); }
    const std::string normalized =
        normalize_absolute_path(target, target_base);

    if (*normalized_root == "/") {
        if (normalized == "/") { return fail(errc::not_found); }
        return normalized.substr(1U);
    }
    if (normalized.compare(0U, normalized_root->size(), *normalized_root) !=
            0U ||
        normalized.size() <= normalized_root->size() + 1U ||
        normalized[normalized_root->size()] != '/') {
        return fail(errc::not_found);
    }
    return std::string(normalized.substr(normalized_root->size() + 1U));
}

/// Copies the target of one symbolic link into caller-owned storage.
///
/// The buffer grows through interrupted calls and truncation until the whole
/// target fits, bounded well beyond any realistic path length before
/// reporting value_too_large instead of growing forever.
inline result<std::string> read_link_target(const char* path) {
    constexpr std::size_t maximum_target_size = 8192U;
    std::string buffer(256U, '\0');
    for (;;) {
        const ssize_t written =
            ::readlink(path, &buffer[0], static_cast<size_t>(buffer.size()));
        if (written < 0) {
            if (errno == EINTR) { continue; }
            return fail(std::error_code(errno, std::generic_category()));
        }
        const std::size_t size = static_cast<std::size_t>(written);
        if (size < buffer.size()) {
            buffer.resize(size);
            return buffer;
        }
        if (buffer.size() >= maximum_target_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() * 2U);
    }
}

inline result<std::string> identifier_for_zone_file(
    std::string_view filespec, std::string_view zoneinfo_root);

/// Returns the identifier recorded by the system-wide localtime link.
inline result<std::string> configured_localtime_identifier() {
    const result<std::string> target = read_link_target(localtime_link);
    if (!target) {
        if (target.error() ==
            std::error_code(ENOENT, std::generic_category())) {
            // localtime(5) documents that a missing configuration selects
            // the default UTC zone, so UTC is the recorded configuration
            // rather than a fabricated value.
            return std::string("UTC");
        }
        if (target.error() ==
            std::error_code(EINVAL, std::generic_category())) {
            // A regular-file or hard-link configuration exposes no
            // extractable identifier because the documented extraction
            // relies on the target name.
            return fail(errc::not_found);
        }
        return target;
    }

    // TZDIR affects relative TZ file specifications, not the system-wide
    // configuration selected when TZ itself is absent. Resolve the link
    // against /etc before validating that it names a usable TZif file.
    const std::string resolved = normalize_absolute_path(*target, "/etc");
    return identifier_for_zone_file(resolved, default_zoneinfo_root);
}

/// Resolves one documented geographical TZ file specification to its
/// identifier.
///
/// A relative specification names data below the system time-zone directory;
/// an absolute specification names data directly. Either way the referenced
/// data must exist and its resolved location must remain below a documented
/// zoneinfo root, because only such locations yield an identifier; an
/// escaping or foreign specification records none and reports not_found.
/// Missing data reports not_found; other native failures propagate
/// unchanged.
inline result<std::string> identifier_for_zone_file(
    std::string_view filespec, std::string_view zoneinfo_root) {
    const result<std::string> normalized_root =
        absolute_zoneinfo_root(zoneinfo_root);
    if (!normalized_root) { return fail(normalized_root.error()); }

    std::string target;
    if (!filespec.empty() && filespec.front() == '/') {
        target = normalize_absolute_path(filespec, "/");
    } else {
        target = normalize_absolute_path(filespec, *normalized_root);
    }
    const result<std::string> identifier =
        zone_identifier_from_target(target, *normalized_root, "/");
    if (!identifier) { return fail(identifier.error()); }

    const int descriptor =
        ::open(target.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (descriptor < 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    const linux_platform::file_descriptor owned_descriptor(descriptor);
    static_cast<void>(owned_descriptor);

    struct ::stat status {};
    if (::fstat(descriptor, &status) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (!S_ISREG(status.st_mode)) { return fail(errc::malformed_data); }

    char magic[4];
    std::size_t used = 0U;
    while (used < sizeof(magic)) {
        const ssize_t count =
            ::read(descriptor, magic + used, sizeof(magic) - used);
        if (count > 0) {
            used += static_cast<std::size_t>(count);
            continue;
        }
        if (count == 0) { return fail(errc::malformed_data); }
        if (errno != EINTR) {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
    if (std::memcmp(magic, "TZif", sizeof(magic)) != 0) {
        return fail(errc::malformed_data);
    }

    return identifier;
}

/// Returns the platform's recorded local time-zone identifier.
///
/// An unset TZ observes the system-wide localtime link, while glibc's
/// documented empty-value extension selects UTC. A geographical TZ value
/// names the effective zone file with or without the optional leading colon
/// and yields its identifier, honoring the glibc-documented TZDIR directory
/// override. A non-file value is a POSIX rule string, which records no
/// identifier, so the query reports not_found instead of fabricating a name.
inline result<std::string> time_zone_identifier() {
    const char* const override_value = ::getenv("TZ");
    if (override_value == nullptr) { return configured_localtime_identifier(); }
    const std::string_view spec(override_value);
    if (spec.empty() || spec == ":") { return std::string("UTC"); }

    const std::string_view filespec =
        spec.front() == ':' ? spec.substr(1U) : spec;
    const char* const configured_root = ::getenv("TZDIR");
    const std::string_view root =
        configured_root != nullptr && configured_root[0] != '\0'
            ? std::string_view(configured_root)
            : std::string_view(default_zoneinfo_root);
    return identifier_for_zone_file(filespec, root);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
