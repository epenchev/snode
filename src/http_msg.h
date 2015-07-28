//
// http_msg.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef HTTP_MSG_H_
#define HTTP_MSG_H_

#include <map>
#include <memory>
#include <string>
#include <queue>
#include <vector>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <functional>

#include "http_headers.h"
#include "async_streams.h"
#include "async_task.h"
#include "producer_consumer_buf.h"
#include "container_buffer.h"

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

    typedef streams::async_streambuf<uint8_t, streams::producer_consumer_buffer<uint8_t>> streambuf_type;
    typedef typename streambuf_type::istream_type istream_type;
    typedef typename streambuf_type::ostream_type ostream_type;

    http_msg_base() : data_available_(0) {}

    virtual ~http_msg_base() {}

    /// Gets the headers of the (response/request) message.
    http_headers& headers()
    {
        return headers_;
    }

    /// Generates a string representation of the message, including the body when possible.
    virtual std::string to_string();

    /// Extract a string from the body or the whole body.
    std::string extract_string(bool ignore_content_type = false);

    /* json::value extract_json(bool ignore_content_type = false); */
    std::vector<unsigned char> extract_vector();

    /// Helper function for extract functions. Parses the Content-Type header and check to make sure it matches,
    /// throws an exception if not.
    /// (ignore_content_type) If true ignores the Content-Type header value.
    /// (check_content_type) Function to verify additional information on Content-Type.
    /// returns a string containing the charset, an empty string if no Content-Type header is empty.
    std::string parse_and_check_content_type(bool ignore_content_type, const std::function<bool(const std::string&)> &check_content_type);

    void set_body(streambuf_type::istream_type& instream, std::size_t length, const std::string& content_type);

    /// Sets the body of the message to a textual string and set the "Content-Type" header.
    void set_body(streambuf_type::istream_type& instream, const std::string& content_type);

    /// Sets the body of the message to a textual string and set the "Content-Type" header.
    template<typename CollectionType>
    void set_body(CollectionType body, std::size_t length, const std::string& content_type)
    {
        streams::container_buffer<CollectionType> sourcebuf(body, std::ios_base::out | std::ios_base::in);
        streams::producer_consumer_buffer<uint8_t> destbuf(body.size());
        headers().set_content_length(length);
        // do copy ? TODO
        istream_type istream = destbuf.create_istream();
        set_body(istream, content_type);
    }

    /// Determine the content length returns
    /// size_t::max if there is content with unknown length (transfer_encoding:chunked)
    /// 0           if there is no content
    /// length      if there is content with known length
    /// This routine should only be called after a msg (request/response) has been completely constructed.
    std::size_t get_content_length();

    /// Completes this message
    void complete(std::size_t body_size);

    /// Set the stream through which the message body could be read
    void set_instream(const istream_type& instream)  { instream_ = instream; }

    /// Set the stream through which the message body could be written
    void set_outstream(const ostream_type& outstream)  { outstream_ = outstream; }

    /// Get the stream through which the message body could be written
    ostream_type& outstream() { return outstream_; }

    /// Get the stream through which the message body could be read
    istream_type& instream() { return instream_; }

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
    /// Thus instream_ is valid when there is a msg body and it can actually be read
    streambuf_type::istream_type instream_;

    /// stream to write the msg body
    /// By default this is an invalid stream. The user could set this on the response
    /// (for http_client). In all the other cases we would construct one to transfer
    /// the data from the network into the message body.
    streambuf_type::ostream_type outstream_;

    http_headers headers_;

    std::size_t data_available_;
};


/// Internal representation of an HTTP response.
class http_response_impl : public http::http_msg_base
{
public:
    http_response_impl() : status_code_((std::numeric_limits<uint16_t>::max)()) { }

    http_response_impl(http::status_code code) : status_code_(code) {}

    http::status_code status_code() const { return status_code_; }

    void set_status_code(http::status_code code) { status_code_ = code; }

    const http::reason_phrase& reason_phrase() const { return reason_phrase_; }

    void set_reason_phrase(const http::reason_phrase &reason) { reason_phrase_ = reason; }

    std::string to_string() const;

private:

    http::status_code status_code_;
    http::reason_phrase reason_phrase_;
};


/// Represents an HTTP response.
class http_response
{
public:

    /// Constructs a response with an empty status code, no headers, and no body.
    http_response() : impl_(std::make_shared<http::http_response_impl>()) { }

    /// Constructs a response with given status code, no headers, and no body.
    http_response(http::status_code code) : impl_(std::make_shared<http::http_response_impl>(code)) { }

    /// Gets the status code of the response message.
    http::status_code status_code() const { return impl_->status_code(); }

    /// Sets the status code of the response message. This will overwrite any previously set status code.
    void set_status_code(http::status_code code) { impl_->set_status_code(code); }

    /// Gets the reason phrase of the response message.
    /// If no reason phrase is set it will default to the standard one corresponding to the status code.
    const http::reason_phrase& reason_phrase() const { return impl_->reason_phrase(); }

    /// Sets the reason phrase of the response message.
    /// If no reason phrase is set it will default to the standard one corresponding to the status code.
    void set_reason_phrase(const http::reason_phrase &reason) { impl_->set_reason_phrase(reason); }

    /// Gets the headers of the response message.
    /// <returns>HTTP headers for this response.</returns>
    /// Use the http_headers::add() Method to fill in desired headers.
    http_headers &headers() { return impl_->headers(); }

    /// Gets a const reference to the headers of the response message.
    const http_headers &headers() const { return impl_->headers(); }

    /// Generates a string representation of the message, including the body when possible.
    /// Mainly this should be used for debugging purposes as it has to copy the
    /// message body and doesn't have excellent performance.

    /// <returns>A string representation of this HTTP request.</returns>
    /// <remarks>Note this function is synchronous and doesn't wait for the
    /// entire message body to arrive. If the message body has arrived by the time this
    /// function is called and it is has a textual Content-Type it will be included.
    /// Otherwise just the headers will be present.</remarks>
    std::string to_string() const { return impl_->to_string(); }

    /// Extracts the body of the response message as a string value, checking that the content type is a MIME text type.
    /// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    /// (ignore_content_type) If true, ignores the Content-Type header.
    /// Returns string containing body of the message.
    std::string extract_string(bool ignore_content_type = false)
    {
        if (impl_->get_data_available())
            return impl_->extract_string(ignore_content_type);
        else
            return std::string();
    }

#if 0
    /// Extracts the body of the request message into a json value, checking that the content type is application\json.
    /// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    /// (ignore_content_type) If true, ignores the Content-Type header.
    /// Returns JSON value from the body of this message.
    task<json::value> extract_json(bool ignore_content_type = false) const
    {
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl, ignore_content_type](utility::size64_t) { return impl->_extract_json(ignore_content_type); });
    }
#endif

    /// Extracts the body of the response message into a vector of bytes.
    /// Returns the body of the message as a vector of bytes.
    std::vector<unsigned char> extract_vector()
    {
        if (impl_->get_data_available())
            return impl_->extract_vector();
        else
            throw std::runtime_error("No data available");
    }

    /// Sets the body of the message to a textual string and set the "Content-Type" header.
    /// (body_text) String containing body text.
    /// (content_type) MIME type to set the "Content-Type" header to. Default to "text/plain".
    /// This will overwrite any previously set body data and "Content-Type" header.
    void set_body(const std::string& body_text, std::string content_type = "text/plain")
    {
        if (content_type.find("charset=") != content_type.npos)
        {
            throw std::invalid_argument("content_type can't contain a 'charset'.");
        }
        impl_->set_body(body_text, (size_t)body_text.size(), content_type);
    }

#if 0
    /// Sets the body of the message to contain json value. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/json'.
    /// This will overwrite any previously set body data.
    void set_body(const json::value &body_data)
    {
        auto body_text = utility::conversions::to_utf8string(body_data.serialize());
        auto length = body_text.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_text)), length, _XPLATSTR("application/json"));
    }
#endif


    /// Sets the body of the message to the contents of a byte vector. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/octet-stream'.
    void set_body(std::vector<unsigned char> &&body_data, const std::string& content_type = "application/octet-stream")
    {
        impl_->set_body(std::move(body_data), (size_t)body_data.size(), content_type);
    }


    /// Sets the body of the message to the contents of a byte vector. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/octet-stream'.
    void set_body(const std::vector<unsigned char>& body_data, const std::string& content_type = "application/octet-stream")
    {
        impl_->set_body(body_data, (size_t)body_data.size(), content_type);
    }

    /// Defines a stream that will be relied on to provide the body of the HTTP message when it is sent.
    /// (stream) - A readable, open asynchronous stream.
    /// This cannot be used in conjunction with any other means of setting the body of the request.
    /// The stream will not be read until the message is sent.
    void set_body(http_msg_base::istream_type& stream, const std::string &content_type = ("application/octet-stream"))
    {
        impl_->set_body(stream, content_type);
    }

    /// Defines a stream that will be relied on to provide the body of the HTTP message when it is sent.
    /// (stream) - A readable, open asynchronous stream.
    /// (content_length) - The size of the data to be sent in the body.
    /// (content_type) - A string holding the MIME type of the message body.
    /// This cannot be used in conjunction with any other means of setting the body of the request.
    /// The stream will not be read until the message is sent.
    void set_body(http_msg_base::istream_type& stream, std::size_t content_length, const std::string& content_type = "application/octet-stream")
    {
        impl_->set_body(stream, content_length, content_type);
    }

    /// Produces a stream which the caller may use to retrieve data from an incoming request.

    /// This cannot be used in conjunction with any other means of getting the body of the request.
    /// It is not necessary to wait until the message has been sent before starting to write to the
    /// stream, but it is advisable to do so, since it will allow the network I/O to start earlier
    /// and the work of sending data can be overlapped with the production of more data.
    http_msg_base::istream_type body() const
    {
        return impl_->instream();
    }

    /// Signals the user (client) when all the data for this response message has been received.
    void content_ready() const
    {
        /*
        http_response resp = *this;
        return pplx::create_task(_m_impl->_get_data_available()).then([resp](utility::size64_t) mutable { return resp; });
        */
    }

    std::shared_ptr<http::http_response_impl> get_impl() const { return impl_; }

private:
    std::shared_ptr<http::http_response_impl> impl_;
};

class http_request_impl_op
{
public:
    void response_ready(http::http_response response)
    {
        func_(this, response);
    }

    protected:
        typedef void (*func_type)(http_request_impl_op*, http::http_response);
        http_request_impl_op(func_type func) : func_(func) {}

    private:
        func_type func_;
};

template<typename Handler>
class async_http_request_handler : public http_request_impl_op
{
public:

    async_http_request_handler(Handler h)
        : http_request_impl_op(&async_http_request_handler::do_get_response), handler_(h) {}

    static void do_get_response(http_request_impl_op* base, http::http_response response)
    {
        async_http_request_handler* op = static_cast<async_http_request_handler*>(base);
        op->handler_(response);
    }

private:
    Handler handler_;
};


/// Internal representation of an HTTP request message.
class http_request_impl : public http_msg_base
{
    public:
        http_request_impl(http::method mtd) : method_(mtd), response_ready_(false), initiated_response_(false) {}

        virtual ~http_request_impl() {}

        http::method& method() { return method_; }

        std::string& request_url() { return url_; }

        void set_request_url(const std::string& url) { url_ = url; }

        std::string to_string() const;

        template <typename ResponseHandler>
        void async_get_response(ResponseHandler handler)
        {
            if (!response_ready_)
            {
                async_http_request_handler<ResponseHandler> op(handler);
                response_handlers_.push(op);
            }
            else
            {
                async_event_task::connect(handler, response_);
            }
        }

        void reply_if_not_already(http::status_code status);

        void reply(http::http_response& response);

    private:

        std::string url_;
        http::method method_;
        bool response_ready_;
        bool initiated_response_;
        http::http_response response_;
        std::queue<http_request_impl_op> response_handlers_;
};

/// Represents an HTTP request.
class http_request
{
public:

    /// Constructs a new HTTP request with the 'GET' method.
    http_request() : impl_(std::make_shared<http::http_request_impl>(http::methods::GET)) {}

    /// Constructs a new HTTP request with the given request method.
    http_request(http::method mtd) : impl_(std::make_shared<http::http_request_impl>(std::move(mtd))) {}

    /// Get the method (GET/PUT/POST/DELETE) of the request message.
    const http::method& method() const { return impl_->method(); }

    /// Get the method (GET/PUT/POST/DELETE) of the request message.
    void set_method(const http::method& mtd) { impl_->method() = mtd; }

    /// Get the underling URL of the request message.
    const std::string& request_url() const { return impl_->request_url(); }

    /// Set the underling URL of the request message.
    void set_request_url(const std::string& url) { impl_->set_request_url(url); }

    /// Gets a reference to the headers of the message.
    /// Use the http_headers::add to fill in desired headers.
    http_headers &headers() { return impl_->headers(); }

    /// Gets a const reference to the headers of the message.
    const http_headers& headers() const { return impl_->headers(); }

    /// Extracts the body of the response message as a string value, checking that the content type is a MIME text type.
    /// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    /// (ignore_content_type) If true, ignores the Content-Type header.
    /// Returns string containing body of the message.
    std::string extract_string(bool ignore_content_type = false)
    {
        if (impl_->get_data_available())
            return impl_->extract_string(ignore_content_type);
        else
            return std::string();
    }

#if 0
    /// Extracts the body of the request message into a json value, checking that the content type is application\json.
    /// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    /// (ignore_content_type) If true, ignores the Content-Type header.
    /// Returns JSON value from the body of this message.
    task<json::value> extract_json(bool ignore_content_type = false) const
    {
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl, ignore_content_type](utility::size64_t) { return impl->_extract_json(ignore_content_type); });
    }
#endif

    /// Extracts the body of the response message into a vector of bytes.
    /// Returns the body of the message as a vector of bytes.
    std::vector<unsigned char> extract_vector()
    {
        if (impl_->get_data_available())
            return impl_->extract_vector();
        else
            throw std::runtime_error("No data available");
    }

    /// Sets the body of the message to a textual string and set the "Content-Type" header.
    /// (body_text) String containing body text.
    /// (content_type) MIME type to set the "Content-Type" header to. Default to "text/plain".
    /// This will overwrite any previously set body data and "Content-Type" header.
    void set_body(const std::string& body_text, std::string content_type = "text/plain")
    {
        if (content_type.find("charset=") != content_type.npos)
        {
            throw std::invalid_argument("content_type can't contain a 'charset'.");
        }
        impl_->set_body(body_text, (size_t)body_text.size(), content_type);
    }

#if 0
    /// Sets the body of the message to contain json value. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/json'.
    /// This will overwrite any previously set body data.
    void set_body(const json::value& body_data)
    {
        auto body_text = utility::conversions::to_utf8string(body_data.serialize());
        auto length = body_text.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_text)), length, _XPLATSTR("application/json"));
    }
#endif

    /// Sets the body of the message to the contents of a byte vector. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/octet-stream'.
    void set_body(std::vector<unsigned char> &&body_data, const std::string& content_type = "application/octet-stream")
    {
        impl_->set_body(std::move(body_data), (size_t)body_data.size(), content_type);
    }

    /// Sets the body of the message to the contents of a byte vector. If the 'Content-Type'
    /// header hasn't already been set it will be set to 'application/octet-stream'.
    void set_body(const std::vector<unsigned char>& body_data, const std::string& content_type = "application/octet-stream")
    {
        impl_->set_body(body_data, (size_t)body_data.size(), content_type);
    }

    /// Defines a stream that will be relied on to provide the body of the HTTP message when it is sent.
    /// (stream) - A readable, open asynchronous stream.
    /// This cannot be used in conjunction with any other means of setting the body of the request.
    /// The stream will not be read until the message is sent.
    void set_body(http_msg_base::istream_type& stream, const std::string &content_type = ("application/octet-stream"))
    {
        impl_->set_body(stream, content_type);
    }

    /// Defines a stream that will be relied on to provide the body of the HTTP message when it is sent.
    /// (stream) - A readable, open asynchronous stream.
    /// (content_length) - The size of the data to be sent in the body.
    /// (content_type) - A string holding the MIME type of the message body.
    /// This cannot be used in conjunction with any other means of setting the body of the request.
    /// The stream will not be read until the message is sent.
    void set_body(http_msg_base::istream_type& stream, std::size_t content_length, const std::string& content_type = "application/octet-stream")
    {
        impl_->set_body(stream, content_length, content_type);
    }

    /// Produces a stream which the caller may use to retrieve data from an incoming request.

    /// This cannot be used in conjunction with any other means of getting the body of the request.
    /// It is not necessary to wait until the message has been sent before starting to write to the
    /// stream, but it is advisable to do so, since it will allow the network I/O to start earlier
    /// and the work of sending data can be overlapped with the production of more data.
    http_msg_base::istream_type body() const
    {
        return impl_->instream();
    }

    /// Defines a stream that will be relied on to hold the body of the HTTP response message that
    /// results from the request.
    /// If this function is called, the body of the response should not be accessed in any other way.
    void set_response_stream(const http_msg_base::ostream_type& stream)
    {
        /* return _m_impl->set_response_stream(stream); */
    }


    /// Asynchronously responses to this HTTP request.
    /// <param name="response">Response to send.</param>
    void reply(http_response& response)
    {
        impl_->reply(response);
    }

    /// Asynchronously responses to this HTTP request.
    /// (status) Response status code.
    void reply(http::status_code status)
    {
        http::http_response response(status);
        reply(response);
    }

#if 0
    /// Responds to this HTTP request.
    /// (status) Response status code.
    /// (body_data) Json value to use in the response body.
    void reply(http::status_code status, const json::value& body_data)
    {
        http::http_response response(status);
        response.set_body(body_data);
        reply(response);
    }
#endif

    /// Responds to this HTTP request with a string. Assumes the character encoding
    /// of the string is UTF-16 will perform conversion to UTF-8.
    /// (status) Response status code.
    /// (body_data) A string containing the text to use in the response body.
    /// (content_type) Content type of the body.
    void reply(http::status_code status, const std::string& body_data, const std::string& content_type = "text/plain")
    {
        http_response response(status);
        response.set_body(body_data, content_type);
        return reply(response);
    }

    /// Responds to this HTTP request.
    /// (status) Response status code.
    /// (content_type) A string holding the MIME type of the message body.
    /// (body) An asynchronous stream representing the body data.
    void reply(status_code status, http_msg_base::istream_type& body, const std::string& content_type = "application/octet-stream")
    {
        http_response response(status);
        response.set_body(body, content_type);
        return reply(response);
    }

    /// Responds to this HTTP request.
    /// (status) Response status code.
    /// (content_length) The size of the data to be sent in the body.
    /// (content_type) A string holding the MIME type of the message body.
    /// (body) An asynchronous stream representing the body data.
    void reply(status_code status, http_msg_base::istream_type& body, std::size_t content_length, const std::string& content_type = "application/octet-stream")
    {
        http_response response(status);
        response.set_body(body, content_length, content_type);
        return reply(response);
    }

    /// Sends a response if one has not already been sent.
    void reply_if_not_already(http::status_code status) { impl_->reply_if_not_already(status); }

    std::shared_ptr<http::http_request_impl> get_impl() { return impl_; }

    private:
        std::shared_ptr<http::http_request_impl> impl_;
};

}}

#endif // HTTP_MSG_H_

