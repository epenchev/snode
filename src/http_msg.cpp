//
// http_msg.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include "http_msg.h"
#include "http_headers.h"
#include <limits>
#include <stdexcept>

namespace smkit { namespace http
{

#define CRLF std::string("\r\n")

std::string http_headers::content_type() const
{
    std::string result;
    match(http::header_names::content_type, result);
    return result;
}

void http_headers::set_content_type(const std::string& type)
{
    m_headers[http::header_names::content_type] = type;
}

std::string http_headers::cache_control() const
{
    std::string result;
    match(http::header_names::cache_control, result);
    return result;
}

void http_headers::set_cache_control(const std::string& control)
{
    add(http::header_names::cache_control, control);
}

std::string http_headers::date() const
{
    std::string result;
    match(http::header_names::date, result);
    return result;
}

/* TODO
void http_headers::set_date(const utility::datetime& date)
{
    m_headers[http::header_names::date] = date.to_string(utility::datetime::RFC_1123);
}
*/

std::size_t http_headers::content_length() const
{
    std::size_t length = 0;
    match(http::header_names::content_length, length);
    return length;
}

void http_headers::set_content_length(std::size_t length)
{
    /* m_headers[http::header_names::content_length] = util_conversions::print_string(length); TODO */
}

http_msg_base::http_msg_base()
    : //m_headers(),
      m_default_outstream(false)
{
}

std::size_t http_msg_base::get_content_length()
{
    // An invalid response_stream indicates that there is no body
    //if ((bool)instream())
    //{
        size_t content_length = 0;
        std::string transfer_encoding;

        bool has_cnt_length = headers().match(header_names::content_length, content_length);
        bool has_xfr_encode = headers().match(header_names::transfer_encoding, transfer_encoding);

        if (has_xfr_encode)
            return std::numeric_limits<size_t>::max();

        if (has_cnt_length)
            return content_length;

        // Neither is set. Assume transfer-encoding for now (until we have the ability to determine
        // the length of the stream).
        headers().add(header_names::transfer_encoding, "chunked");
        return std::numeric_limits<size_t>::max();
    //}

    return 0;
}

/* TODO
json::value details::http_msg_base::_extract_json(bool ignore_content_type)
{
    const auto &charset = parse_and_check_content_type(ignore_content_type, is_content_type_json);
    if (charset.empty())
    {
        return json::value();
    }
    auto buf_r = instream().streambuf();

    // Latin1
    if(utility::details::str_icmp(charset, charset_types::latin1))
    {
        std::string body;
        body.resize(buf_r.in_avail());
        buf_r.getn(const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(body.data())), body.size()).get(); // There is no risk of blocking.
        // On Linux could optimize in the future if a latin1_to_utf8 function is written.
        return json::value::parse(to_string_t(latin1_to_utf16(std::move(body))));
    }

    // utf-8, usascii and ascii
    else if(utility::details::str_icmp(charset, charset_types::utf8)
            || utility::details::str_icmp(charset, charset_types::usascii)
            || utility::details::str_icmp(charset, charset_types::ascii))
    {
        std::string body;
        body.resize(buf_r.in_avail());
        buf_r.getn(const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(body.data())), body.size()).get(); // There is no risk of blocking.
        return json::value::parse(to_string_t(std::move(body)));
    }

    // utf-16.
    else if(utility::details::str_icmp(charset, charset_types::utf16))
    {
        utf16string body;
        body.resize(buf_r.in_avail() / sizeof(utf16string::value_type));
        buf_r.getn(const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(body.data())), body.size() * sizeof(utf16string::value_type)); // There is no risk of blocking.
        return json::value::parse(convert_utf16_to_string_t(std::move(body)));
    }

    // utf-16le
    else if(utility::details::str_icmp(charset, charset_types::utf16le))
    {
        utf16string body;
        body.resize(buf_r.in_avail() / sizeof(utf16string::value_type));
        buf_r.getn(const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(body.data())), body.size() * sizeof(utf16string::value_type)); // There is no risk of blocking.
        return json::value::parse(convert_utf16le_to_string_t(std::move(body), false));
    }

    // utf-16be
    else if(utility::details::str_icmp(charset, charset_types::utf16be))
    {
        utf16string body;
        body.resize(buf_r.in_avail() / sizeof(utf16string::value_type));
        buf_r.getn(const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(body.data())), body.size() * sizeof(utf16string::value_type)); // There is no risk of blocking.
        return json::value::parse(convert_utf16be_to_string_t(std::move(body), false));
    }

    else
    {
        throw http_exception(unsupported_charset);
    }
}
*/

/* TODO
std::vector<uint8_t> details::http_msg_base::_extract_vector()
{
    if (!instream())
    {
        throw http_exception(stream_was_set_explicitly);
    }

    std::vector<uint8_t> body;
    auto buf_r = instream().streambuf();
    const size_t size = buf_r.in_avail();
    body.resize(size);
    buf_r.getn(const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(body.data())), size).get(); // There is no risk of blocking.

    return body;
}
*/


//
// Helper function to generate a wstring from given http_headers and message body.
//
/* TODO
static std::string http_headers_body_to_string(const http_headers &headers, concurrency::streams::istream instream)
{
    utility::ostringstream_t buffer;
    buffer.imbue(std::locale::classic());

    for (const auto &header : headers)
    {
        buffer << header.first << _XPLATSTR(": ") << header.second << CRLF;
    }
    buffer << CRLF;

    utility::string_t content_type;
    if(headers.match(http::header_names::content_type, content_type))
    {
        buffer << convert_body_to_string_t(content_type, instream);
    }

    return buffer.str();
}
*/

/* TODO
std::string http_msg_base::to_string() const
{
    return http_headers_body_to_string(m_headers, instream());
}
*/

static void set_content_type_if_not_present(http::http_headers& headers, const std::string& content_type)
{
    std::string temp;
    if(!headers.match(http::header_names::content_type, temp))
    {
        headers.add(http::header_names::content_type, content_type);
    }
}

/* TODO
void http_msg_base::set_body(const streams::istream &instream, const utf8string &contentType)
{
    set_content_type_if_not_present(
    		headers(),
#ifdef _UTF16_STRINGS
    		utility::conversions::utf8_to_utf16(contentType));
#else
    		contentType);
#endif
    set_instream(instream);
}
*/


/* TODO
void details::http_msg_base::set_body(const streams::istream &instream, const utf16string &contentType)
{
    set_content_type_if_not_present(
    		headers(),
#ifdef _UTF16_STRINGS
    		contentType);
#else
    		utility::conversions::utf16_to_utf8(contentType));
#endif
    set_instream(instream);
}
*/


/* TODO
void details::http_msg_base::set_body(const streams::istream &instream, utility::size64_t contentLength, const utf8string &contentType)
{
    headers().set_content_length(contentLength);
    set_body(instream, contentType);
    m_data_available.set(contentLength);
}
*/

/* TODO
void details::http_msg_base::set_body(const concurrency::streams::istream &instream, utility::size64_t contentLength, const utf16string &contentType)
{
    headers().set_content_length(contentLength);
    set_body(instream, contentType);
    m_data_available.set(contentLength);
}
*/


http_request::http_request(http::method mtd)
  : m_method(mtd)
{
    if (m_method.empty())
        throw std::runtime_error("Invalid HTTP method specified. Method can't be an empty string.");
}


#define _METHODS
#define DAT(a,b) const method methods::a = b;
#include "http_constants.dat"
#undef _METHODS
#undef DAT

#define _HEADER_NAMES
#define DAT(a,b) const std::string header_names::a = (b);
#include "http_constants.dat"
#undef _HEADER_NAMES
#undef DAT
/*
#define _MIME_TYPES
#define DAT(a,b) const std::string mime_types::a = b;
#include "http_constants.dat"
#undef _MIME_TYPES
#undef DAT
*/
/*
#define _CHARSET_TYPES
#define DAT(a,b) const std::string charset_types::a = b;
#include "http_constants.dat"
#undef _CHARSET_TYPES
#undef DAT
*/
// This is necessary for Linux because of a bug in GCC 4.7
#ifndef _WIN32
#define _PHRASES
#define DAT(a,b,c) const status_code status_codes::a;
#include "http_constants.dat"
#undef _PHRASES
#undef DAT
#endif

}} // namespace smkit::http
