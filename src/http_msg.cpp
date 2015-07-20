//
// http_msg.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include "http_msg.h"
#include "http_headers.h"
#include <limits>
#include <stdexcept>

namespace snode
{
namespace http
{

#define CRLF std::string("\r\n")

static void set_content_type_if_not_present(http::http_headers& headers, const std::string& content_type)
{
    std::string temp;
    if (!headers.match(http::header_names::content_type, temp))
    {
        headers.add(http::header_names::content_type, content_type);
    }
}

std::string http_headers::content_type() const
{
    std::string result;
    match(http::header_names::content_type, result);
    return result;
}

void http_headers::set_content_type(const std::string& type)
{
    headers_[http::header_names::content_type] = type;
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

void http_headers::set_date()
{
    headers_[http::header_names::date] = utility::current_date_time();
}


std::size_t http_headers::content_length() const
{
    std::size_t length = 0;
    match(http::header_names::content_length, length);
    return length;
}

void http_headers::set_content_length(std::size_t length)
{
    headers_[http::header_names::content_length] = utility::conversions::print_string(length);
}

void http_msg_base::set_body(istream_type& instream, const std::string& content_type)
{
    set_content_type_if_not_present(headers_, content_type);
    set_instream(instream);
}

void http_msg_base::set_body(istream_type& instream, std::size_t length, const std::string& content_type)
{
    headers().set_content_length(length);
    set_body(instream, content_type);
    data_available_ = length;
}

void http_msg_base::complete(std::size_t body_size)
{

}

std::size_t http_msg_base::get_content_length()
{
    // An invalid response_stream indicates that there is no body
    if ((bool)instream())
    {
        size_t content_length = 0;
        std::string transfer_encoding;

        bool has_cnt_length = headers_.match(header_names::content_length, content_length);
        bool has_xfr_encode = headers_.match(header_names::transfer_encoding, transfer_encoding);

        if (has_xfr_encode)
        {
            return std::numeric_limits<size_t>::max();
        }

        if (has_cnt_length)
        {
            return content_length;
        }

        // Neither is set. Assume transfer-encoding for now (until we have the ability to determine
        // the length of the stream).
        headers_.add(header_names::transfer_encoding, "chunked");
        return std::numeric_limits<size_t>::max();
    }

    return 0;
}

static std::string convert_body_to_string(const std::string& content_type, http_msg_base::istream_type instream)
{
    return ""; // for now
}

/// Helper function to generate a string from given http_headers and message body.
static std::string http_headers_body_to_string(const http_headers &headers, http_msg_base::istream_type instream)
{
    std::ostringstream buffer;
    buffer.imbue(std::locale::classic());

    for (const auto &header : headers)
    {
        /* buffer << header.first << _XPLATSTR(": ") << header.second << CRLF; */
    }

    buffer << CRLF;

    std::string content_type;
    if (headers.match(http::header_names::content_type, content_type))
    {
        buffer << convert_body_to_string(content_type, instream);
    }

    return buffer.str();
}

std::string http_msg_base::to_string() const
{
    /* return http_headers_body_to_string(headers_, instream()); */
    return "";
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

}}
