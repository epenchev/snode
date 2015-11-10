//
// http_msg.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include "http_msg.h"
#include "http_helpers.h"
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
    headers_[http::header_names::date] = utility::details::current_date_time();
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

static const std::string stream_was_set_explicitly = ("A stream was set on the message and extraction is not possible");

void http_msg_base::set_body(streambuf_type::istream_type& instream, const std::string& content_type)
{
    set_content_type_if_not_present(headers_, content_type);
    set_instream(instream);
}

void http_msg_base::set_body(streambuf_type::istream_type& instream, std::size_t length, const std::string& content_type)
{
    headers().set_content_length(length);
    set_body(instream, content_type);
    data_available_ = length;

    while (!data_ready_handlers_.empty())
    {
        auto event_op = data_ready_handlers_.front();
        async_task::connect(&data_ready_op::data_ready, event_op, data_available_);
        data_ready_handlers_.pop();
    }
}

void http_msg_base::complete(std::size_t body_size)
{
    data_available_ = body_size;
    outstream().close();
}

void http_msg_base::prepare_to_receive_data()
{
    // See if the user specified an outstream
    if (!outstream())
    {
        // The user did not specify an outstream.
        // We will create one...
        http_msg_base::streambuf_type buf(std::ios_base::out & std::ios_base::in);
        set_outstream(buf.create_ostream());

        // Since we are creating the streambuffer, set the input stream
        // so that the user can retrieve the data.
        set_instream(buf.create_istream());
    }
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

static std::string convert_body_to_string(const std::string& content_type, http_msg_base::streambuf_type::istream_type instream)
{
    if (!instream)
    {
        // The instream is not set
        return std::string();
    }

    http_msg_base::streambuf_type streambuf = instream.streambuf();

    assert(streambuf.is_open());
    assert(streambuf.can_read());

    std::string content, charset;
    parse_content_type_and_charset(content_type, content, charset);

    // Content-Type must have textual type.
    if (!is_content_type_textual(content) || streambuf.in_avail() == 0)
    {
        return std::string();
    }

    if (streambuf.in_avail() > 0)
    {
        std::string body;
        body.resize(streambuf.in_avail());
        if(streambuf.scopy((unsigned char *)&body[0], body.size()) == 0)
            return std::string();
        else
            return body;
    }
    else
    {
        return std::string();
    }
}

/// Helper function to generate a string from given http_headers and message body.
static std::string http_headers_body_to_string(const http_headers &headers,
                                               http_msg_base::streambuf_type::istream_type instream)
{
    std::ostringstream buffer;
    buffer.imbue(std::locale::classic());

    for (const auto &header : headers)
        buffer << header.first << ": " << header.second << CRLF;
    buffer << CRLF;

    std::string content_type;
    if (headers.match(http::header_names::content_type, content_type))
        buffer << convert_body_to_string(content_type, instream);

    return buffer.str();
}

std::string http_msg_base::to_string()
{
    if (instream().is_valid())
        return http_headers_body_to_string(headers_, instream());
    else
        return std::string();
}

std::string http_msg_base::parse_and_check_content_type(bool ignore_content_type,
                                                        const std::function<bool(const std::string&)> &check_content_type)
{
    if (!instream())
    {
        throw http_exception(stream_was_set_explicitly);
    }

    std::string content, charset = charset_types::utf8;
    if (!ignore_content_type)
    {
        parse_content_type_and_charset(headers().content_type(), content, charset);

        // If no Content-Type or empty body then just return an empty string.
        if (content.empty() || instream().streambuf().in_avail() == 0)
        {
            return std::string();
        }

        if (!check_content_type(content))
        {
            throw http_exception("Incorrect Content-Type: must be textual to extract_string, JSON to extract_json.");
        }
    }
    return charset;
}

std::string http_msg_base::extract_string(bool ignore_content_type)
{
    const auto& charset = parse_and_check_content_type(ignore_content_type, is_content_type_textual);

    if (charset.empty())
    {
        return std::string();
    }

    auto buf_r = instream().streambuf();

    if (buf_r.in_avail() > 0)
    {
        std::string body;
        body.resize(buf_r.in_avail());
        if(buf_r.scopy((unsigned char *)&body[0], body.size()) == 0)
            return std::string();
        else
            return body;
    }
    return std::string();
}

std::vector<uint8_t> http_msg_base::extract_vector()
{
    if (!instream())
    {
        throw http_exception(stream_was_set_explicitly);
    }

    std::vector<uint8_t> body;
    auto buf_r = instream().streambuf();

    auto avail = buf_r.in_avail();
    if (avail)
    {
        unsigned idx = 0;
        auto buf_r = instream().streambuf();
        body.resize(avail);

        while (avail && buf_r.in_avail() > 0)
        {
            body[idx++] = buf_r.sbumpc();
            avail--;
        }
    }
    return body;
}

void http_request_impl::reply_impl(http::http_response& response)
{
    if (initiated_response_)
    {
        throw http_exception("Error: trying to send multiple responses to an HTTP request");
    }
    initiated_response_ = true;

    // If the user didn't explicitly set a reason phrase then we should have it default
    // if they used one of the standard known status codes.
     if (response.reason_phrase().empty())
     {
         static http_status_to_phrase id_phrase_map[] =
         {
#define _PHRASES
#define DAT(a,b,c) {status_codes::a, c},
#include "http_constants.dat"
#undef _PHRASES
#undef DAT
         };

         for (const auto& iter : id_phrase_map)
         {
             if (iter.id == response.status_code())
             {
                 response.set_reason_phrase(iter.phrase);
                 break;
             }
         }
     }

     response_ = response;
     response_ready_ = true;
     while (!response_handlers_.empty())
     {
         auto op = response_handlers_.front();
         // We have enough data to satisfy this request
         async_task::connect(&response_ready_op::response_ready, op, std::ref(response_));
         response_handlers_.pop();
     }
}

void http_request_impl::reply_if_not_already(http::status_code status)
{
    if (!initiated_response_)
    {
        http::http_response response(status);
        reply_impl(response);
    }
}

void http_request_impl::response_send_complete(http::error_code& err)
{
    while (!response_complete_handlers_.empty())
    {
        auto op = response_complete_handlers_.front();
        async_task::connect(&response_complete_op::response_complete, op, err);
        response_complete_handlers_.pop();
    }
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

#define _MIME_TYPES
#define DAT(a,b) const std::string mime_types::a = b;
#include "http_constants.dat"
#undef _MIME_TYPES
#undef DAT


#define _CHARSET_TYPES
#define DAT(a,b) const std::string charset_types::a = b;
#include "http_constants.dat"
#undef _CHARSET_TYPES
#undef DAT

// This is necessary for Linux because of a bug in GCC 4.7
#ifndef _WIN32
#define _PHRASES
#define DAT(a,b,c) const status_code status_codes::a;
#include "http_constants.dat"
#undef _PHRASES
#undef DAT
#endif

}}
