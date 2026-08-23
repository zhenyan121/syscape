#ifndef SYSCAPE_DETAIL_NETWORK_WINDOWS_HPP
#define SYSCAPE_DETAIL_NETWORK_WINDOWS_HPP

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

#include <syscape/detail/network/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace network_backend {

/// Converts UTF-16 interface text to UTF-8 at the platform boundary.
inline result<std::string> wide_to_utf8(std::wstring_view value) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t),
                  "The Windows backend requires 16-bit wchar_t");
    std::u16string converted;
    converted.reserve(value.size());
    for (wchar_t unit : value) {
        converted.push_back(static_cast<char16_t>(unit));
    }
    return utf16_to_utf8(converted);
}

/// Owns a GetAdaptersAddresses table and releases it through the platform
/// API that produced it.
template <typename AdapterApi>
class adapter_table_guard {
public:
    explicit adapter_table_guard(::IP_ADAPTER_ADDRESSES* value) noexcept
        : value_(value) {}
    adapter_table_guard(const adapter_table_guard&) = delete;
    adapter_table_guard& operator=(const adapter_table_guard&) = delete;
    ~adapter_table_guard() {
        if (value_ != nullptr) { AdapterApi::release(value_); }
    }

    ::IP_ADAPTER_ADDRESSES* get() const noexcept { return value_; }

private:
    ::IP_ADAPTER_ADDRESSES* value_;
};

/// Platform calls used to enumerate network adapters.
///
/// The indirection exists so tests can drive enumeration with synthetic
/// adapter tables instead of real hardware; production callers always use
/// the native implementation.
struct native_adapter_api {
    /// Returns an owned GetAdaptersAddresses table for every adapter,
    /// growing the caller-allocated buffer while the platform reports
    /// overflow.
    ///
    /// The documented buffer-growth contract updates Size on overflow, so
    /// the loop converges; once the required size exceeds a fixed ceiling
    /// the loop stops and surfaces the native overflow code instead of
    /// growing without bound. ERROR_NO_DATA maps to not_found because the
    /// platform itself reports that no adapter information exists.
    static result<::IP_ADAPTER_ADDRESSES*> adapters() {
        constexpr ::ULONG initial_size = 15U * 1024U;
        constexpr ::ULONG maximum_size = 16U * 1024U * 1024U;
        ::ULONG size = initial_size;
        for (;;) {
            ::IP_ADAPTER_ADDRESSES* table =
                static_cast<::IP_ADAPTER_ADDRESSES*>(::malloc(size));
            if (table == nullptr) { return fail(errc::resource_exhausted); }
            const ::ULONG outcome =
                ::GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX,
                                       nullptr, table, &size);
            if (outcome == ERROR_SUCCESS) { return table; }
            ::free(table);
            if (outcome == ERROR_NO_DATA) {
                return fail(errc::not_found);
            }
            if (outcome == ERROR_BUFFER_OVERFLOW && size <= maximum_size) {
                continue;
            }
            return fail(std::error_code(static_cast<int>(outcome),
                                        std::system_category()));
        }
    }

    /// Releases a table obtained from adapters().
    static void release(::IP_ADAPTER_ADDRESSES* table) noexcept {
        ::free(table);
    }
};

/// Returns the portable state for a documented adapter operational status.
inline network_common::interface_state classify_oper_status(
    ::IF_OPER_STATUS status) noexcept {
    if (status == IfOperStatusUp) {
        return network_common::interface_state::up;
    }
    if (status == IfOperStatusDown) {
        return network_common::interface_state::down;
    }
    return network_common::interface_state::unknown;
}

/// Returns the portable name for one adapter row.
///
/// The friendly name is the operating system's user-facing interface label
/// and is reported as the interface name after UTF-8 conversion. Where an
/// adapter records none, the ANSI adapter identifier is used verbatim so
/// that every record still carries a usable name; an adapter with neither
/// is malformed platform data.
inline result<std::string> adapter_name(const ::IP_ADAPTER_ADDRESSES& row) {
    if (row.FriendlyName != nullptr && row.FriendlyName[0] != L'\0') {
        return wide_to_utf8(std::wstring_view(row.FriendlyName));
    }
    if (row.AdapterName == nullptr || row.AdapterName[0] == '\0') {
        return fail(errc::malformed_data);
    }
    return std::string(row.AdapterName);
}

/// Converts one unicast-address entry of an adapter row.
///
/// Address families this slice does not represent are skipped without
/// failing the query. A prefix length beyond its family's range is
/// malformed platform data because no documented producer emits one.
inline result<void> convert_unicast_entry(
    const ::IP_ADAPTER_UNICAST_ADDRESS& entry,
    network_common::interface_record& target) {
    if (entry.Address.lpSockaddr == nullptr ||
        entry.Address.iSockaddrLength <
            static_cast<int>(sizeof(::sockaddr))) {
        return fail(errc::malformed_data);
    }
    const unsigned int family =
        static_cast<unsigned int>(entry.Address.lpSockaddr->sa_family);

    network_common::unicast_record record;
    if (family == static_cast<unsigned int>(AF_INET)) {
        if (entry.Address.iSockaddrLength <
            static_cast<int>(sizeof(::sockaddr_in))) {
            return fail(errc::malformed_data);
        }
        const ::sockaddr_in* address =
            reinterpret_cast<const ::sockaddr_in*>(entry.Address.lpSockaddr);
        record.family = network_common::address_family::ipv4;
        const unsigned char* bytes =
            reinterpret_cast<const unsigned char*>(&address->sin_addr);
        for (std::size_t offset = 0U; offset < 4U; ++offset) {
            record.value[offset] = bytes[offset];
        }
    } else if (family == static_cast<unsigned int>(AF_INET6)) {
        if (entry.Address.iSockaddrLength <
            static_cast<int>(sizeof(::sockaddr_in6))) {
            return fail(errc::malformed_data);
        }
        const ::sockaddr_in6* address =
            reinterpret_cast<const ::sockaddr_in6*>(entry.Address.lpSockaddr);
        record.family = network_common::address_family::ipv6;
        const unsigned char* bytes =
            reinterpret_cast<const unsigned char*>(&address->sin6_addr);
        for (std::size_t offset = 0U; offset < 16U; ++offset) {
            record.value[offset] = bytes[offset];
        }
        record.scope_id = static_cast<std::uint32_t>(address->sin6_scope_id);
    } else {
        return {};
    }

    const std::uint64_t prefix =
        static_cast<std::uint64_t>(entry.OnLinkPrefixLength);
    const std::uint64_t maximum = static_cast<std::uint64_t>(
        network_common::maximum_prefix_length(record.family));
    if (prefix > maximum) { return fail(errc::malformed_data); }
    record.prefix_length = static_cast<std::uint8_t>(prefix);
    target.addresses.push_back(std::move(record));
    return {};
}

/// Converts one adapter row into the shared record shape.
inline result<network_common::interface_record> convert_adapter_row(
    const ::IP_ADAPTER_ADDRESSES& row) {
    result<std::string> name = adapter_name(row);
    if (!name) { return fail(name.error()); }

    network_common::interface_record record;
    record.name = std::move(*name);
    // IfIndex is documented as zero when IPv4 is unavailable. Preserve a
    // usable operating-system index for IPv6-only adapters instead of
    // rejecting that valid platform state as malformed data.
    record.index = static_cast<std::uint32_t>(
        row.IfIndex != 0U ? row.IfIndex : row.Ipv6IfIndex);
    if (record.index == 0U) { return fail(errc::not_supported); }
    record.state = classify_oper_status(row.OperStatus);
    record.loopback = row.IfType == IF_TYPE_SOFTWARE_LOOPBACK;
    record.mtu_bytes = static_cast<std::uint32_t>(row.Mtu);

    // The platform stores the address verbatim in a fixed-size buffer and
    // records how many bytes are meaningful; those bytes are copied without
    // reinterpretation, including none where the recorded length is zero.
    // A recorded length beyond the documented fixed buffer cannot be read
    // safely and is malformed platform data.
    if (row.PhysicalAddressLength >
        sizeof(row.PhysicalAddress) / sizeof(row.PhysicalAddress[0])) {
        return fail(errc::malformed_data);
    }
    record.hardware_address.assign(row.PhysicalAddress,
                                   row.PhysicalAddress +
                                       row.PhysicalAddressLength);

    for (const ::IP_ADAPTER_UNICAST_ADDRESS* entry = row.FirstUnicastAddress;
         entry != nullptr; entry = entry->Next) {
        const result<void> converted = convert_unicast_entry(*entry, record);
        if (!converted) { return fail(converted.error()); }
    }
    return record;
}

/// Converts a whole adapter table into interface records.
inline result<std::vector<network_common::interface_record>>
convert_adapter_table(const ::IP_ADAPTER_ADDRESSES* table) {
    std::vector<network_common::interface_record> interfaces;
    for (const ::IP_ADAPTER_ADDRESSES* cursor = table; cursor != nullptr;
         cursor = cursor->Next) {
        result<network_common::interface_record> record =
            convert_adapter_row(*cursor);
        if (!record) { return fail(record.error()); }
        interfaces.push_back(std::move(*record));
    }
    return interfaces;
}

/// Enumerates every adapter through the given platform API.
template <typename AdapterApi>
inline result<std::vector<network_common::interface_record>> enumerate(
    AdapterApi /*api*/) {
    const result<::IP_ADAPTER_ADDRESSES*> table = AdapterApi::adapters();
    if (!table) { return fail(table.error()); }
    const adapter_table_guard<AdapterApi> guard(*table);
    return convert_adapter_table(guard.get());
}

/// Returns a snapshot of the platform's network interfaces and their
/// unicast addresses through the documented GetAdaptersAddresses interface.
inline result<std::vector<network_common::interface_record>> interfaces() {
    return enumerate(native_adapter_api{});
}

} // namespace network_backend
} // namespace detail
} // namespace syscape

#endif
