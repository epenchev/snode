//
// http_msg.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef HTTP_MSG_H_
#define HTTP_MSG_H_

#include <http_headers.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace smkit
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
public:
    /// Creates an http_exception with just a string message.
    http_exception(const std::string &msg) : m_msg(msg)
    {
    }
    /// Gets a string identifying the cause of the exception.
    const char* what() const throw()
    {
        return m_msg.c_str();
    }

    virtual ~http_exception() throw()
    {
    }
private:
    std::string m_msg;
};


/// Base class for HTTP messages. This class is to store common functionality so it isn't duplicated on
/// both the request and response side.
class http_msg_base
{
public:
    http_msg_base();

    virtual ~http_msg_base()
    {
    }

    /// Gets the headers of the (response/request) message.
    http_headers& headers()
    {
        return m_headers;
    }

    /// Generates a string representation of the message, including the body when possible.
    virtual std::string to_string() const { return ""; }

    /// Sets the body of the message to a textual string and set the "Content-Type" header.
    /// void set_body(const concurrency::streams::istream &instream, utility::size64_t contentLength, const utf8string &contentType);

    /// Extracts the body of the response message into a json value, checking that the content type is application\json.
    /// json::value _extract_json(bool ignore_content_type = false);

    /// Extracts the body of the response message into a json value, checking that the content type is application\json.
    // std::vector<unsigned char> _extract_vector();

    /// Determine the content length returns
    /// size_t::max if there is content with unknown length (transfer_encoding:chunked)
    /// 0           if there is no content
    /// length      if there is content with known length
    /// This routine should only be called after a msg (request/response) has been completely constructed.
    std::size_t get_content_length();

protected:

    /// <summary>
    /// Stream to read the message body.
    /// By default this is an invalid stream. The user could set the instream on
    /// a request by calling set_request_stream(...). This would also be set when
    /// set_body() is called - a stream from the body is constructed and set.
    /// Even in the presense of msg body this stream could be invalid. An example
    /// would be when the user sets an ostream for the response. With that API the
    /// user does not provide the ability to read the msg body.
    /// Thus m_instream is valid when there is a msg body and it can actually be read
    /// </summary>
    /* concurrency::streams::istream m_inStream; */

    /// <summary>
    /// stream to write the msg body
    /// By default this is an invalid stream. The user could set this on the response
    /// (for http_client). In all the other cases we would construct one to transfer
    /// the data from the network into the message body.
    /// </summary>
    /* concurrency::streams::ostream m_outStream; */

    http_headers m_headers;
    bool m_default_outstream;
};

/// Represents an HTTP response.
class http_response : public http::http_msg_base
{
public:
    /// Constructs a response with an empty status code, no headers, and no body.
    /// returns a new HTTP response.
    http_response() : m_status_code(0)
    {
    }

    /// Constructs a response with given status code, no headers, and no body.
    /// returns  new HTTP response.
    http_response(http::status_code code) : m_status_code(code)
    {
    }

    virtual ~http_response()
    {
    }

    /// Gets the status code of the response message.
    http::status_code status_code() const
    {
        return m_status_code;
    }

    /// Sets the status code of the response message. This will overwrite any previously set status code.
    void set_status_code(http::status_code code)
    {
        m_status_code = code;
    }

    /// Gets the reason phrase of the response message.
    /// If no reason phrase is set it will default to the standard one corresponding to the status code.
    const http::reason_phrase& reason_phrase() const
    {
        return m_reason_phrase;
    }

    /// Sets the reason phrase of the response message.
    /// If no reason phrase is set it will default to the standard one corresponding to the status code.
    void set_reason_phrase(const http::reason_phrase &reason)
    {
        m_reason_phrase = reason;
    }


    /// <summary>
    /// Sets the body of the message to contain json value. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/json'.
    /// </summary>
    /// <param name="body_text">json value.</param>
    /// <remarks>
    /// This will overwrite any previously set body data.
    /// </remarks>
    /*
    void set_body(const json::value &body_data)
    {
        auto body_text = utility::conversions::to_utf8string(body_data.serialize());
        auto length = body_text.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_text)), length, _XPLATSTR("application/json"));
    }
    */

    /// <summary>
    /// Sets the body of the message to the contents of a byte vector. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/octet-stream'.
    /// </summary>
    /// <param name="body_data">Vector containing body data.</param>
    /// <remarks>
    /// This will overwrite any previously set body data.
    /// </remarks>
    /*
    void set_body(std::vector<unsigned char> &&body_data)
    {
        auto length = body_data.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_data)), length);
    }*/

    /// <summary>
    /// Sets the body of the message to the contents of a byte vector. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/octet-stream'.
    /// </summary>
    /// <param name="body_data">Vector containing body data.</param>
    /// <remarks>
    /// This will overwrite any previously set body data.
    /// </remarks>
    /*
    void set_body(const std::vector<unsigned char> &body_data)
    {
        set_body(concurrency::streams::bytestream::open_istream(body_data), body_data.size());
    }
    */

    /// <summary>
    /// Defines a stream that will be relied on to provide the body of the HTTP message when it is
    /// sent.
    /// </summary>
    /// <param name="stream">A readable, open asynchronous stream.</param>
    /// <remarks>
    /// This cannot be used in conjunction with any other means of setting the body of the request.
    /// The stream will not be read until the message is sent.
    /// </remarks>
    /*
    void set_body(const concurrency::streams::istream &stream, const utility::string_t &content_type = _XPLATSTR("application/octet-stream"))
    {
        _m_impl->set_body(stream, content_type);
    }*/

    /// <summary>
    /// Defines a stream that will be relied on to provide the body of the HTTP message when it is
    /// sent.
    /// </summary>
    /// <param name="stream">A readable, open asynchronous stream.</param>
    /// <param name="content_length">The size of the data to be sent in the body.</param>
    /// <param name="content_type">A string holding the MIME type of the message body.</param>
    /// <remarks>
    /// This cannot be used in conjunction with any other means of setting the body of the request.
    /// The stream will not be read until the message is sent.
    /// </remarks>
    /*
    void set_body(const concurrency::streams::istream &stream, utility::size64_t content_length, const utility::string_t &content_type = _XPLATSTR("application/octet-stream"))
    {
        _m_impl->set_body(stream, content_length, content_type);
    }
    */

private:
    http::status_code m_status_code;
    http::reason_phrase m_reason_phrase;
};



/// Represents an HTTP request.
class http_request : public http::http_msg_base
{
public:

    /// Constructs a new HTTP request with the 'GET' method.
    http_request() : m_method(http::methods::GET) {}

    /// Constructs a new HTTP request with the given request method.
    http_request(http::method mtd);

    virtual ~http_request()
    {
    }

    /// Get the method (GET/PUT/POST/DELETE) of the request message.
    const http::method& method() const
    {
        return m_method;
    }

    /// Get the method (GET/PUT/POST/DELETE) of the request message.
    void set_method(const http::method &method)
    {
        m_method = method;
    }

    /// Get the underling URI of the request message.
    const std::string& request_uri() const { return m_uri; }

    /// Set the underling URI of the request message.
    void set_request_uri(const std::string& uri)
    {
        m_uri = uri;
    }

    /// Gets a reference the URI path, query, and fragment part of this request message.
    /// This will be appended to the base URI specified at construction of the http_client.
    /// When the request is the one passed to a listener's handler, the
    /// relative URI is the request URI less the listener's path. In all other circumstances,
    /// request_uri() and relative_uri() will return the same value.
    // std::string relative_uri() const { return _m_impl->relative_uri(); }


    /// Get an absolute URI with scheme, host, port, path, query, and fragment part of
    /// the request message.
    /// Absolute URI is only valid after this http_request object has been passed to http_client::request().
    // std::string absolute_uri() const { return _m_impl->absolute_uri(); }

    /// <summary>
    /// Asynchronously responses to this HTTP request.
    /// </summary>
    /// <param name="response">Response to send.</param>
    /// <returns>An asynchronous operation that is completed once response is sent.</returns>
    /// pplx::task<void> reply(const http_response &response) const { return _m_impl->reply(response); }

    /// <summary>
    /// Asynchronously responses to this HTTP request.
    /// </summary>
    /// <param name="status">Response status code.</param>
    /// <returns>An asynchronous operation that is completed once response is sent.</returns>
    /*
    pplx::task<void> reply(http::status_code status) const
    {
        return reply(http_response(status));
    }
    */

    /// <summary>
    /// Responds to this HTTP request.
    /// </summary>
    /// <param name="status">Response status code.</param>
    /// <param name="body_data">Json value to use in the response body.</param>
    /// <returns>An asynchronous operation that is completed once response is sent.</returns>
    /*
    pplx::task<void> reply(http::status_code status, const json::value &body_data) const
    {
        http_response response(status);
        response.set_body(body_data);
        return reply(response);
    }
    */

    /// Responds to this HTTP request with a string.
    /// Assumes the character encoding of the string is UTF-8.
    /// </summary>
    /// <param name="status">Response status code.</param>
    /// <param name="body_data">UTF-8 string containing the text to use in the response body.</param>
    /// <param name="content_type">Content type of the body.</param>
    /// <returns>An asynchronous operation that is completed once response is sent.</returns>
    /// <remarks>
    //  Callers of this function do NOT need to block waiting for the response to be
    /// sent to before the body data is destroyed or goes out of scope.
    /// </remarks>
    /*
    pplx::task<void> reply(http::status_code status, utf8string &&body_data, const utf8string &content_type = "text/plain; charset=utf-8") const
    {
        http_response response(status);
        response.set_body(std::move(body_data), content_type);
        return reply(response);
    }
    */

    /// <summary>
    /// Sends a response if one has not already been sent.
    /// </summary>
    /// pplx::task<void> _reply_if_not_already(status_code status) { return _m_impl->_reply_if_not_already(status); }


    /// void _set_listener_path(const utility::string_t &path) { _m_impl->_set_listener_path(path); }

private:
    http::method m_method;
    std::string  m_base_uri;
    std::string  m_uri;
    std::string  m_listener_path;
};
}}

#endif // HTTP_MSG_H_

