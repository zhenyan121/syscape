#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/printer.hpp>
#include <syscape/detail/printer/common.hpp>
#include <syscape/detail/printer/linux.hpp>

namespace {

void append_int_attr(std::vector<std::uint8_t>& buf,
                     std::uint8_t tag,
                     std::string_view name,
                     std::uint32_t val) {
    using namespace syscape::detail::printer_common::ipp;
    buf.push_back(tag);
    append_uint16(buf, static_cast<std::uint16_t>(name.size()));
    for (char c : name) buf.push_back(static_cast<std::uint8_t>(c));
    append_uint16(buf, 4U);
    append_uint32(buf, val);
}

void append_bool_attr(std::vector<std::uint8_t>& buf,
                      std::string_view name,
                      bool val) {
    using namespace syscape::detail::printer_common::ipp;
    buf.push_back(TAG_BOOLEAN);
    append_uint16(buf, static_cast<std::uint16_t>(name.size()));
    for (char c : name) buf.push_back(static_cast<std::uint8_t>(c));
    append_uint16(buf, 1U);
    buf.push_back(val ? 1U : 0U);
}

void append_1setof_string(std::vector<std::uint8_t>& buf,
                          std::uint8_t tag,
                          std::string_view val) {
    using namespace syscape::detail::printer_common::ipp;
    buf.push_back(tag);
    append_uint16(buf, 0U); // name_len = 0 for 1setOf
    append_uint16(buf, static_cast<std::uint16_t>(val.size()));
    for (char c : val) buf.push_back(static_cast<std::uint8_t>(c));
}

void append_text(std::vector<std::uint8_t>& buffer, std::string_view text) {
    for (char value : text) {
        buffer.push_back(static_cast<std::uint8_t>(value));
    }
}

std::vector<std::uint8_t> http_response(
    const std::vector<std::uint8_t>& body) {
    const std::string header =
        "HTTP/1.1 200 OK\r\nContent-Type: application/ipp\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n\r\n";
    std::vector<std::uint8_t> response;
    append_text(response, header);
    response.insert(response.end(), body.begin(), body.end());
    return response;
}

std::vector<std::uint8_t> chunked_http_response(
    const std::vector<std::uint8_t>& body) {
    std::vector<std::uint8_t> response;
    append_text(response,
                "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
    char size_buffer[32];
    const auto converted =
        std::to_chars(size_buffer, size_buffer + sizeof(size_buffer),
                      body.size(), 16);
    assert(converted.ec == std::errc());
    append_text(response,
                std::string_view(size_buffer,
                                 static_cast<std::size_t>(converted.ptr - size_buffer)));
    append_text(response, "\r\n");
    response.insert(response.end(), body.begin(), body.end());
    append_text(response, "\r\n0\r\nTest-Trailer: value\r\n\r\n");
    return response;
}

void test_common_helpers() {
    using namespace syscape::detail::printer_common;

    // trim_whitespace
    assert(trim_whitespace("  hello \t\r\n") == "hello");
    assert(trim_whitespace("").empty());
    assert(trim_whitespace("   ").empty());

    // equals_ignore_case
    assert(equals_ignore_case("PRINTER", "printer"));
    assert(equals_ignore_case("Cups-Pdf", "CUPS-PDF"));
    assert(!equals_ignore_case("printer1", "printer2"));
    assert(!equals_ignore_case("printer", "printers"));
    assert(normalized_printer_identity("HP_DeskJet_4100") ==
           normalized_printer_identity("HP DeskJet 4100"));
    assert(normalized_printer_identity("usb://HP/DeskJet%204100?serial=42") ==
           normalized_printer_identity("HP DeskJet 4100"));

    // natural_less
    assert(natural_less("lp1", "lp2"));
    assert(natural_less("lp2", "lp10"));
    assert(natural_less("printer_01", "printer_2"));
    assert(!natural_less("printer_10", "printer_2"));
    assert(!natural_less("same", "same"));

    // parse_int
    assert(parse_int<int>("123") == 123);
    assert(parse_int<unsigned>("0x1A", 16) == 0x1A);
    assert(parse_int<int>("-123") == -123);
    assert(!parse_int<int>("abc"));
    assert(!parse_int<int>(""));

    // classify_printer_type
    assert(classify_printer_type("usb://HP/LaserJet", "HP LaserJet", "HP") ==
           syscape::printer::printer_type::local);
    assert(classify_printer_type("ipp://192.168.1.100/ipp/print", "Network Printer", "Generic") ==
           syscape::printer::printer_type::network);
    assert(classify_printer_type("socket://192.168.1.50:9100", "Office Jet", "Generic") ==
           syscape::printer::printer_type::network);
    assert(classify_printer_type("cups-pdf:/", "PDF", "CUPS-PDF") ==
           syscape::printer::printer_type::virtual_printer);
    assert(classify_printer_type("PORTPROMPT:", "Microsoft Print to PDF", "PDF Driver") ==
           syscape::printer::printer_type::virtual_printer);
    assert(classify_printer_type("unknown://test", "Custom", "Driver") ==
           syscape::printer::printer_type::unknown);

    // map_ipp_printer_state
    assert(map_ipp_printer_state(3) == syscape::printer::printer_state::idle);
    assert(map_ipp_printer_state(4) == syscape::printer::printer_state::processing);
    assert(map_ipp_printer_state(5) == syscape::printer::printer_state::stopped);
    assert(map_ipp_printer_state(99) == syscape::printer::printer_state::unknown);

    std::vector<syscape::printer::printer_info> defaults(2U);
    defaults[0].name = "Alpha";
    defaults[1].name = "Zulu";
    set_default_by_name(defaults, "Alpha");
    set_default_by_name(defaults, "Zulu");
    assert(defaults[0].is_default == false);
    assert(defaults[1].is_default == true);
    const auto selected = marked_default_printer(defaults);
    assert(selected && selected->name == "Zulu");
    defaults[1].is_default = false;
    assert(!marked_default_printer(defaults));

    defaults[0].is_default.reset();
    const std::optional<std::error_code> changing_default =
        syscape::make_error_code(syscape::errc::temporarily_unavailable);
    const auto unresolved =
        resolve_snapshot_default(defaults, false, changing_default);
    assert(!unresolved);
    assert(unresolved.error() == syscape::errc::temporarily_unavailable);
    assert(defaults.size() == 2U);
}

void test_ipp_request_builder() {
    using namespace syscape::detail::printer_common::ipp;

    const auto req = build_get_printers_request(42);
    assert(req.size() >= 8U);
    assert(req[0] == 0x02 && req[1] == 0x00); // IPP 2.0
    assert(req[2] == 0x40 && req[3] == 0x02); // CUPS-Get-Printers (0x4002)
    assert(req[4] == 0x00 && req[5] == 0x00 && req[6] == 0x00 && req[7] == 42); // Request ID 42
    assert(req[8] == TAG_OPERATION_ATTRIBUTES); // 0x01
    assert(req.back() == TAG_END_OF_ATTRIBUTES); // 0x03
    const auto contains_text = [](const std::vector<std::uint8_t>& bytes,
                                  std::string_view value) {
        return std::search(bytes.begin(), bytes.end(), value.begin(), value.end()) !=
               bytes.end();
    };
    assert(contains_text(req, "requested-attributes"));
    assert(contains_text(req, "device-uri"));
    assert(contains_text(req, "printer-is-accepting-jobs"));
    assert(contains_text(req, "media-supported"));

    const auto def_req = build_get_default_request(1);
    assert(def_req.size() >= 8U);
    assert(def_req[2] == 0x40 && def_req[3] == 0x01); // CUPS-Get-Default (0x4001)
    assert(contains_text(def_req, "requested-attributes"));
    assert(contains_text(def_req, "printer-name"));
}

void test_ipp_response_parser() {
    using namespace syscape::detail::printer_common::ipp;

    // 1. Truncated header
    const std::uint8_t short_buf[] = {0x02, 0x00, 0x00};
    assert(!parse_ipp_printers_response(short_buf, sizeof(short_buf)));

    // 2. Error status in header (e.g. 0x0406 not found)
    const std::uint8_t not_found_buf[] = {
        0x02, 0x00, 0x04, 0x06, 0x00, 0x00, 0x00, 0x01, TAG_END_OF_ATTRIBUTES
    };
    const auto err_res = parse_ipp_printers_response(not_found_buf, sizeof(not_found_buf));
    assert(!err_res);
    assert(err_res.error() == syscape::errc::not_found);

    for (const std::uint16_t status : {std::uint16_t{0x0405},
                                       std::uint16_t{0x0502},
                                       std::uint16_t{0x0507}}) {
        const std::uint8_t unavailable_buf[] = {
            0x02, 0x00,
            static_cast<std::uint8_t>(status >> 8),
            static_cast<std::uint8_t>(status & 0xFFU),
            0x00, 0x00, 0x00, 0x01, TAG_END_OF_ATTRIBUTES};
        const auto unavailable = parse_ipp_printers_response(
            unavailable_buf, sizeof(unavailable_buf));
        assert(!unavailable);
        assert(unavailable.error() == syscape::errc::temporarily_unavailable);
    }

    // 3. Build a valid synthetic IPP response
    std::vector<std::uint8_t> payload;
    // Header: version 2.0, status 0x0000 (successful-ok), req-id 1
    payload.push_back(0x02);
    payload.push_back(0x00);
    append_uint16(payload, 0x0000);
    append_uint32(payload, 1);

    // Group 1: Operation attributes
    payload.push_back(TAG_OPERATION_ATTRIBUTES);
    append_string_attr(payload, TAG_CHARSET, "attributes-charset", "utf-8");
    append_string_attr(payload, TAG_NATURALLANGUAGE, "attributes-natural-language", "en");

    // Group 2: Printer 1 attributes
    payload.push_back(TAG_PRINTER_ATTRIBUTES);
    append_string_attr(payload, TAG_NAME_WITHOUT_LANGUAGE, "printer-name", "Office_LaserJet");
    append_string_attr(payload, TAG_TEXT_WITHOUT_LANGUAGE, "printer-info", "Main Office Printer");
    append_string_attr(payload, TAG_TEXT_WITHOUT_LANGUAGE, "printer-location", "Room 101");
    append_string_attr(payload, TAG_TEXT_WITHOUT_LANGUAGE, "printer-make-and-model", "HP LaserJet Pro");
    append_string_attr(payload, TAG_URI, "device-uri", "socket://192.168.1.50:9100");

    // printer-state = 3 (idle)
    append_int_attr(payload, TAG_ENUM, "printer-state", 3);

    // printer-is-accepting-jobs = true
    append_bool_attr(payload, "printer-is-accepting-jobs", true);

    // printer-is-shared = false
    append_bool_attr(payload, "printer-is-shared", false);

    // queued-job-count = 2
    append_int_attr(payload, TAG_INTEGER, "queued-job-count", 2);

    // color-supported = true
    append_bool_attr(payload, "color-supported", true);

    // media-supported: 1setOf ("iso_a4_210x297mm", "na_letter_8.5x11in")
    append_string_attr(payload, TAG_KEYWORD, "media-supported", "iso_a4_210x297mm");
    append_1setof_string(payload, TAG_KEYWORD, "na_letter_8.5x11in");

    // printer-type bitmask (color | duplex | copies)
    append_int_attr(payload, TAG_INTEGER, "printer-type",
                    CUPS_PRINTER_COLOR | CUPS_PRINTER_DUPLEX | CUPS_PRINTER_COPIES);

    // Group 3: Printer 2 attributes (PDF virtual printer)
    payload.push_back(TAG_PRINTER_ATTRIBUTES);
    append_string_attr(payload, TAG_NAME_WITHOUT_LANGUAGE, "printer-name", "PDF_Writer");
    append_string_attr(payload, TAG_TEXT_WITHOUT_LANGUAGE, "printer-info", "Print to PDF");
    append_string_attr(payload, TAG_URI, "device-uri", "cups-pdf:/");

    // End of attributes
    payload.push_back(TAG_END_OF_ATTRIBUTES);

    const auto parsed = parse_ipp_printers_response(payload.data(), payload.size());
    assert(parsed);
    assert(parsed->size() == 2U);

    const auto& p1 = (*parsed)[0];
    assert(p1.id == "Office_LaserJet");
    assert(p1.name == "Office_LaserJet");
    assert(p1.description == "Main Office Printer");
    assert(p1.location == "Room 101");
    assert(p1.driver_name == "HP LaserJet Pro");
    assert(p1.uri == "socket://192.168.1.50:9100");
    assert(p1.type == syscape::printer::printer_type::network);
    assert(p1.state == syscape::printer::printer_state::idle);
    assert(p1.is_accepting_jobs == true);
    assert(p1.is_shared == false);
    assert(p1.queued_job_count == 2U);
    assert(p1.capabilities.color == true);
    assert(p1.capabilities.duplex == true);
    assert(p1.capabilities.copies == true);
    assert(p1.capabilities.supported_media.size() == 2U);
    assert(p1.capabilities.supported_media[0] == "iso_a4_210x297mm");
    assert(p1.capabilities.supported_media[1] == "na_letter_8.5x11in");

    const auto& p2 = (*parsed)[1];
    assert(p2.id == "PDF_Writer");
    assert(p2.name == "PDF_Writer");
    assert(p2.description == "Print to PDF");
    assert(p2.type == syscape::printer::printer_type::virtual_printer);

    auto truncated = payload;
    truncated.pop_back();
    const auto truncated_result =
        parse_ipp_printers_response(truncated.data(), truncated.size());
    assert(!truncated_result);
    assert(truncated_result.error() == syscape::errc::malformed_data);

    auto invalid_utf8 = payload;
    const auto invalid_position = std::search(
        invalid_utf8.begin(), invalid_utf8.end(),
        reinterpret_cast<const std::uint8_t*>("Office_LaserJet"),
        reinterpret_cast<const std::uint8_t*>("Office_LaserJet") + 15);
    assert(invalid_position != invalid_utf8.end());
    *invalid_position = 0xFFU;
    const auto invalid_encoding =
        parse_ipp_printers_response(invalid_utf8.data(), invalid_utf8.size());
    assert(!invalid_encoding);
    assert(invalid_encoding.error() == syscape::errc::invalid_encoding);

    const auto framed = http_response(payload);
    const auto framed_body =
        syscape::detail::printer_common::parse_http_ipp_response(framed);
    assert(framed_body && *framed_body == payload);

    const auto chunked = chunked_http_response(payload);
    const auto chunked_body =
        syscape::detail::printer_common::parse_http_ipp_response(chunked);
    assert(chunked_body && *chunked_body == payload);

    auto wrong_length = framed;
    wrong_length.pop_back();
    assert(!syscape::detail::printer_common::parse_http_ipp_response(
        wrong_length));

    std::vector<std::uint8_t> forbidden;
    append_text(forbidden, "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");
    const auto forbidden_result =
        syscape::detail::printer_common::parse_http_ipp_response(forbidden);
    assert(!forbidden_result);
    assert(forbidden_result.error() == syscape::errc::permission_denied);
}

void test_cups_printers_conf_parser() {
    using namespace syscape::detail::printer_common;

    const std::string_view sample_conf = R"(
# CUPS printers.conf sample
<Printer HP_DeskJet>
UUID urn:uuid:1234-5678-9abc
Info HP DeskJet 4100 Series
Location Home Office
MakeModel HP DeskJet Plus 4100 series, using PostScript
DeviceURI usb://HP/DeskJet%204100
State Idle
StateTime 1600000000
Accepting Yes
Shared No
</Printer>

<DefaultPrinter Network_Laser>
UUID urn:uuid:5678-1234-def0
Info Office Laser
Location Floor 2
MakeModel Generic PCL Laser Printer
DeviceURI socket://10.0.0.50
State Processing
Accepting Yes
Shared Yes
</DefaultPrinter>
)";

    const auto printers = parse_cups_printers_conf(sample_conf);
    assert(printers);
    assert(printers->size() == 2U);

    const auto& p1 = (*printers)[0];
    assert(p1.id == "HP_DeskJet");
    assert(p1.name == "HP_DeskJet");
    assert(p1.description == "HP DeskJet 4100 Series");
    assert(p1.location == "Home Office");
    assert(p1.driver_name == "HP DeskJet Plus 4100 series, using PostScript");
    assert(p1.uri == "usb://HP/DeskJet%204100");
    assert(p1.type == syscape::printer::printer_type::local);
    assert(p1.state == syscape::printer::printer_state::idle);
    assert(p1.is_accepting_jobs == true);
    assert(p1.is_shared == false);
    assert(p1.is_default == false);

    const auto& p2 = (*printers)[1];
    assert(p2.id == "Network_Laser");
    assert(p2.name == "Network_Laser");
    assert(p2.description == "Office Laser");
    assert(p2.location == "Floor 2");
    assert(p2.driver_name == "Generic PCL Laser Printer");
    assert(p2.uri == "socket://10.0.0.50");
    assert(p2.type == syscape::printer::printer_type::network);
    assert(p2.state == syscape::printer::printer_state::processing);
    assert(p2.is_accepting_jobs == true);
    assert(p2.is_shared == true);
    assert(p2.is_default == true);

    const auto truncated = parse_cups_printers_conf("<Printer Broken>\nInfo value\n");
    assert(!truncated);
    assert(truncated.error() == syscape::errc::malformed_data);

    const std::string invalid_config =
        std::string("<Printer Broken>\nInfo ") + static_cast<char>(0xFF) +
        "\n</Printer>\n";
    const auto invalid_encoding = parse_cups_printers_conf(invalid_config);
    assert(!invalid_encoding);
    assert(invalid_encoding.error() == syscape::errc::invalid_encoding);

    const auto invalid_boolean = parse_cups_printers_conf(
        "<Printer Broken>\nAccepting Maybe\n</Printer>\n");
    assert(!invalid_boolean);
    assert(invalid_boolean.error() == syscape::errc::malformed_data);
}

void test_usb_merge() {
    std::vector<syscape::printer::printer_info> installed(1U);
    installed[0].id = "HP_DeskJet_4100";
    installed[0].name = "HP_DeskJet_4100";
    installed[0].driver_name =
        "HP DeskJet Plus 4100 series, using driverless";
    installed[0].uri = "usb://HP/DeskJet%204100?serial=CN123";
    installed[0].type = syscape::printer::printer_type::local;

    std::vector<syscape::printer::printer_info> discovered(2U);
    discovered[0].id = "lp0";
    discovered[0].name = "HP DeskJet 4100";
    discovered[0].driver_name = "HP DeskJet 4100";
    discovered[0].uri = "/dev/usb/lp0";
    discovered[0].type = syscape::printer::printer_type::local;
    discovered[1].id = "lp1";
    discovered[1].name = "Second Printer";
    discovered[1].uri = "/dev/usb/lp1";
    discovered[1].type = syscape::printer::printer_type::local;

    syscape::detail::printer_backend::merge_usb_printers(
        installed, std::move(discovered));
    assert(installed.size() == 2U);
    assert(installed[1].id == "lp1");
    assert(installed[1].state == syscape::printer::printer_state::unknown);
    assert(!installed[1].is_accepting_jobs.has_value());
}

void test_live_linux_queries() {
    const auto printers_res = syscape::printer::printers();
    if (!printers_res) {
        assert(static_cast<bool>(printers_res.error()));
        return;
    }

    const auto count_res = syscape::printer::printer_count();
    assert(count_res);
    assert(*count_res == printers_res->size());

    const auto def_res = syscape::printer::default_printer();
    if (printers_res->empty()) {
        assert(!def_res);
        assert(def_res.error() == syscape::errc::not_found);
    } else {
        if (def_res) {
            assert(!def_res->name.empty());
        }
    }

    const auto find_nonexistent = syscape::printer::find_printer("__nonexistent_printer_xyz__");
    assert(!find_nonexistent);
    assert(find_nonexistent.error() == syscape::errc::not_found);
}

} // namespace

int main() {
    test_common_helpers();
    test_ipp_request_builder();
    test_ipp_response_parser();
    test_cups_printers_conf_parser();
    test_usb_merge();
    test_live_linux_queries();
    return 0;
}
