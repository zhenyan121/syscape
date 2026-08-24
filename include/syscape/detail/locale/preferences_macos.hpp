#ifndef SYSCAPE_DETAIL_LOCALE_PREFERENCES_MACOS_HPP
#define SYSCAPE_DETAIL_LOCALE_PREFERENCES_MACOS_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_backend {

/// Owns one CoreFoundation object reference for the duration of a query.
class preferences_cf_object {
public:
    explicit preferences_cf_object(::CFTypeRef value) noexcept : value_(value) {}
    preferences_cf_object(const preferences_cf_object&) = delete;
    preferences_cf_object& operator=(const preferences_cf_object&) = delete;
    ~preferences_cf_object() {
        if (value_ != nullptr) { ::CFRelease(value_); }
    }

    /// Returns the owned reference.
    ::CFTypeRef get() const noexcept { return value_; }

private:
    ::CFTypeRef value_;
};

/// Copies one CoreFoundation string into UTF-8 storage.
///
/// A string that cannot be rendered as UTF-8 reports a conversion failure
/// instead of corrupted text.
inline result<std::string> copy_preferences_utf8_string(::CFStringRef value) {
    if (value == nullptr) { return fail(errc::io_error); }
    const ::CFIndex length = ::CFStringGetLength(value);
    if (length == 0) { return std::string(); }
    const ::CFIndex maximum = ::CFStringGetMaximumSizeForEncoding(
        length, ::kCFStringEncodingUTF8);
    if (maximum <= 0) { return fail(errc::io_error); }
    std::string output;
    output.resize(static_cast<std::size_t>(maximum));
    ::CFIndex used = 0;
    const ::CFIndex converted = ::CFStringGetBytes(
        value, ::CFRangeMake(0, length), ::kCFStringEncodingUTF8, 0U,
        false, reinterpret_cast<UInt8*>(&output[0]), maximum, &used);
    if (converted != length || used < 0 || used > maximum) {
        return fail(errc::invalid_encoding);
    }
    output.resize(static_cast<std::size_t>(used));
    return output;
}

/// Platform calls used to read user locale and time-zone preferences.
///
/// The indirection exists so tests can drive collection with synthetic
/// CoreFoundation values instead of the real user session; production
/// callers always use the native implementation.
struct native_preferences_api {
    /// Returns an owned copy of the recorded preferred-languages array.
    ///
    /// The platform reports failures only as null references, which carry
    /// no standard error category, so a null array maps to io_error.
    /// An empty array is valid backend output whose emptiness the public
    /// boundary rejects as unusable platform data.
    static result<::CFArrayRef> preferred_languages() {
        ::CFArrayRef languages = ::CFLocaleCopyPreferredLanguages();
        if (languages == nullptr) { return fail(errc::io_error); }
        return languages;
    }

    /// Returns a retained copy of the country or region code recorded by
    /// the current locale, or not_found when the locale records none.
    static result<::CFTypeRef> region_value() {
        const preferences_cf_object current(::CFLocaleCopyCurrent());
        if (current.get() == nullptr) { return fail(errc::io_error); }
        // CFLocaleGetValue returns an unretained reference owned by the
        // locale, so the value is retained to survive the locale guard.
        const ::CFTypeRef value = ::CFLocaleGetValue(
            static_cast<::CFLocaleRef>(current.get()),
            ::kCFLocaleCountryCode);
        if (value == nullptr) { return fail(errc::not_found); }
        ::CFRetain(value);
        return value;
    }

    /// Reports that CoreFoundation exposes no honest system-zone identifier.
    ///
    /// CFTimeZoneCopySystem substitutes GMT when the system zone cannot be
    /// determined and exposes no way to distinguish that fallback from a
    /// genuinely configured GMT zone. Returning that plausible substitute
    /// would violate the portable query contract.
    static result<::CFTypeRef> time_zone_name() {
        return fail(errc::not_supported);
    }
};

/// Collects one recorded list of preferred language identifiers through the
/// given preferences API.
///
/// Entries are copied verbatim in the platform's recorded preference order;
/// a non-string element is malformed platform data.
template <typename PreferencesApi>
inline result<std::vector<std::string>> collect_preferred_languages() {
    const result<::CFArrayRef> array = PreferencesApi::preferred_languages();
    if (!array) { return fail(array.error()); }
    const preferences_cf_object owned(*array);
    if (::CFGetTypeID(owned.get()) != ::CFArrayGetTypeID()) {
        return fail(errc::malformed_data);
    }
    const ::CFArrayRef languages = static_cast<::CFArrayRef>(owned.get());

    std::vector<std::string> collected;
    const ::CFIndex count = ::CFArrayGetCount(languages);
    collected.reserve(static_cast<std::size_t>(count));
    for (::CFIndex index = 0; index < count; ++index) {
        const ::CFTypeRef element =
            ::CFArrayGetValueAtIndex(languages, index);
        if (element == nullptr ||
            ::CFGetTypeID(element) != ::CFStringGetTypeID()) {
            return fail(errc::malformed_data);
        }
        result<std::string> language = copy_preferences_utf8_string(
            static_cast<::CFStringRef>(element));
        if (!language) { return fail(language.error()); }
        collected.push_back(std::move(*language));
    }
    return collected;
}

/// Copies one recorded single-string fact through the given preferences
/// API, rejecting wrong-typed records as malformed platform data.
template <typename PreferencesApi>
inline result<std::string> collect_single_string(
    const result<::CFTypeRef>& value) {
    if (!value) { return fail(value.error()); }
    const preferences_cf_object owned(*value);
    if (::CFGetTypeID(owned.get()) != ::CFStringGetTypeID()) {
        return fail(errc::malformed_data);
    }
    return copy_preferences_utf8_string(
        static_cast<::CFStringRef>(owned.get()));
}

template <typename PreferencesApi>
inline result<std::string> collect_country_region_code() {
    return collect_single_string<PreferencesApi>(
        PreferencesApi::region_value());
}

template <typename PreferencesApi>
inline result<std::string> collect_time_zone_identifier() {
    return collect_single_string<PreferencesApi>(
        PreferencesApi::time_zone_name());
}

/// Returns the ordered list of language identifiers the user prefers,
/// reported verbatim from the platform's own vocabulary.
inline result<std::vector<std::string>> preferred_languages() {
    return collect_preferred_languages<native_preferences_api>();
}

/// Returns the country or region code recorded by the current locale,
/// reported verbatim from the platform's own vocabulary.
inline result<std::string> country_region_code() {
    return collect_country_region_code<native_preferences_api>();
}

/// Returns the identifier of the system-configured local time zone,
/// or not_supported because CoreFoundation's GMT fallback is ambiguous.
inline result<std::string> time_zone_identifier() {
    return collect_time_zone_identifier<native_preferences_api>();
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
