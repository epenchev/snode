//
// http_msg.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef HTTP_MSG_H_
#define HTTP_MSG_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <exception>
#include <stdexcept>

#include "http_headers.h"
#include "async_streams.h"
#include "producer_consumer_buf.h"

namespace snode
{
namespace http
{

/// Predefined method strings for the standard HTTP methods mentioned in the HTTP 1.1 specification.
typedef std::string method;

/// Common HTTP methods.
class methods
{
public:
#define _METHODS
#define DAT(a,b) const static method a;
#include "http_constants.dat"
#undef _METHODS
#undef DAT
};

typedef unsigned short status_code;


/// Predefined values for all of the standard HTTP 1.1 response status codes.
class status_codes
{
public:
#define _PHRASES
#define DAT(a,b,c) const static status_code a=b;
#include "http_constants.dat"
#undef _PHRASES
#undef DAT
};

typedef std::string reason_phrase;

struct http_status_to_phrase
{
    unsigned short id;
    reason_phrase phrase;
};

/// Constants for the HTTP headers mentioned in RFC 2616.
class header_names
{
public:
#define _HEADER_NAMES
#define DAT(a,b) const static std::string a;
#include "http_constants.dat"
#undef _HEADER_NAMES
#undef DAT
};

/// Represents an HTTP error. This class holds an error message.
class http_exception : public std::exception
{
    std::string msg_;
public:
    /// Creates an http_exception with just a string message.
    http_exception(const std::string &msg) : msg_(msg) {}

    virtual ~http_exception() throw() {}

    /// Gets a string identifying the cause of the exception.
    const char* what() const throw()
    {
        return msg_.c_str();
    }
};


/// Base class for HTTP messages. This class is to store common functionality so it isn't duplicated on
/// both the request and response side.
class http_msg_base
{
public:
    typedef typename streams::async_istream<uint8_t, streams::producer_consumer_buffer<uint8_t>> istream_type;
    typedef typename streams::async_ostream<uint8_t, streams::producer_consumer_buffer<uint8_t>> ostream_type;

    http_msg_base() : data_available_(0) {}

    virtual ~http_msg_base() {}

    /// Gets the headers of the (response/request) message.
    http_headers& headers()
    {
        return headers_;
    }

    /// Generates a string representation of the message, including the body when possible.
    virtual std::string to_string() const;

    /// Sets the body of the message to a textual string and set the "Content-Type" header.
    void set_body(istream_type& instream, const std::string& contentType);

    /// Sets the body of the message to a textual string and set the "Content-Type" header.
    void set_body(istream_type& instream, std::size_t length, const std::string& contentType);

    /// Determine the content length returns
    /// size_t::max if there is content with unknown length (transfer_encoding:chunked)
    /// 0           if there is no content
    /// length      if there is content with known length
    /// This routine should only be called after a msg (request/response) has been completely constructed.
    std::size_t get_content_length();

    /// Completes this message
    void complete(std::size_t body_size);

    /// Set the stream through which the message body could be read
    void set_instream(const http_msg_base::istream_type& instream)  { /* instream_ = instream; */ }

    /// Get the stream through which the message body could be read
    istream_type& instream() { /* return instream_; */ }

    /// Set the stream through which the message body could be written
    void set_outstream(http_msg_base::ostream_type& outstream, bool is_default)  { /* outstream_ = outstream; */ }

    /// Get the stream through which the message body could be written
    ostream_type& outstream() { /* return outstream_;*/ }

    std::size_t get_data_available() const { return data_available_; }

    /// Prepare the message with an output stream to receive network data
    void prepare_to_receive_data();

protected:


    /// Stream to read the message body.
    /// By default this is an invalid stream. The user could set the instream on
    /// a request by calling set_request_stream(...). This would also be set when
    /// set_body() is called - a stream from the body is constructed and set.
    /// Even in the presense of msg body this stream could be invalid. An example
    /// would be when the user sets an ostream for the response. With that API the
    /// user does not provide the ability to read the msg body.
    /// Thus m_instream is valid when there is a msg body and it can actually be read

    /// stream to write the msg body
    /// By default this is an invalid stream. The user could set this on the response
    /// (for http_client). In all the other cases we would construct one to transfer
    /// the data from the network into the message body.
    /* concurrency::streams::ostream m_outStream; */

#if 0
    istream_type instream_;
    ostream_type outstream_;
#endif
    http_headers headers_;

    std::size_t data_available_;
};

/// Represents an HTTP response.
class http_response : public http::http_msg_base
{
public:

    /// Constructs a response with an empty status code, no headers, and no body.
    http_response() : status_code_(0) {}

    /// Constructs a response with given status code, no headers, and no body.
    http_response(http::status_code code) : status_code_(code) {}

    /// Gets the status code of the response message.
    http::status_code status_code() const
    {
        return status_code_;
    }

    /// Sets the status code of the response message. This will overwrite any previously set status code.
    void set_status_code(http::status_code code)
    {
        status_code_ = code;
    }

    /// Gets the reason phrase of the response message.
    /// If no reason phrase is set it will default to the standard one corresponding to the status code.
    const http::reason_phrase& reason_phrase() const
    {
        return reason_phrase_;
    }

    /// Sets the reason phrase of the response message.
    /// If no reason phrase is set it will default to the standard one corresponding to the status code.
    void set_reason_phrase(const http::reason_phrase &reason)
    {
        reason_phrase_ = reason;
    }

private:
    http::status_code status_code_;
    http::reason_phrase reason_phrase_;
};



/// Represents an HTTP request.
class http_request : public http::http_msg_base
{
public:

    /// Constructs a new HTTP request with the 'GET' method.
    http_request() : method_(http::methods::GET) {}

    /// Constructs a new HTTP request with the given request method.
    http_request(http::method mtd);

    /// Get the method (GET/PUT/POST/DELETE) of the request message.
    const http::method& method() const
    {
        return method_;
    }

    /// Get the method (GET/PUT/POST/DELETE) of the request message.
    void set_method(const http::method& mtd)
    {
        method_ = mtd;
    }

    /// Get the underling URI of the request message.
    const std::string& request_uri() const { return uri_; }

    /// Set the underling URI of the request message.
    void set_request_uri(const std::string& uri)
    {
        uri_ = uri;
    }

    /// Asynchronously responses to this HTTP request with HTTP response (response).
    void reply(const http_response &response) const {}

    /// Asynchronously responses to this HTTP request with HTTP status code (status).
    void reply(http::status_code status) const
    {
        return reply(http_response(status));
    }

    /// Responds to this HTTP request with a string.
    /// (status) Response status code.
    /// (body_data) string containing the text to use in the response body.
    /// (content_type) Content type of the body.

    void reply(http::status_code status, const std::string& body_data, const std::string &content_type = "text/plain; charset=utf-8")
    {
        http_response response(status);
        /* response.set_body(std::move(body_data), content_type); */
        reply(response);
    }

    /// Sends a response if one has not already been sent.
    void reply_if_not_already(status_code status) {}

    void set_listener_path(const std::string& path) {}

private:
    http::method method_;
    std::string  uri_;
    std::string  listener_path_;
};
}}

#endif // HTTP_MSG_H_

