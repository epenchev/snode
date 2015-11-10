//
// http_service.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef HTTP_SERVICE_H_
#define HTTP_SERVICE_H_

#include <set>
#include <map>
#include <string>
#include <boost/asio.hpp>

#include "http_msg.h"
#include "net_service.h"
#include "net_service_helpers.h"
#include "snode_types.h"
#include "handler_allocator.h"

namespace snode
{
namespace http
{

class http_service;
class http_listener;

/// HTTP server session.
class http_connection :  public boost::enable_shared_from_this<http_connection>
{
private:
    snode::handler_allocator allocator_; // using the default allocator
    tcp_socket_ptr socket_;
    boost::asio::streambuf request_buf_;
    boost::asio::streambuf response_buf_;
    http_service* p_service_;
    http_listener* p_listener_;
    http_request request_;
    size_t read_, write_;
    size_t read_size_, write_size_;
    bool close_;
    bool chunked_;
    thread_id_t worker_id_;
    
public:
    http_connection(tcp_socket_ptr socket, http_service* service, http_listener* listener, thread_id_t id) : socket_(socket), request_buf_()
    , response_buf_(), p_service_(service), p_listener_(listener), worker_id_(id)
    {
        start_request_response();
    }

    http_connection(const http_connection&) = delete;
    http_connection& operator=(const http_connection&) = delete;

    void close();

private:
    void start_request_response();
    void handle_http_line(const boost::system::error_code& ec);
    void handle_headers();
    void handle_body(const boost::system::error_code& ec);
    void handle_chunked_header(const boost::system::error_code& ec);
    void handle_chunked_body(const boost::system::error_code& ec, int toWrite);
    void dispatch_request_to_listener();
    void do_response(bool bad_request);
    template <typename ReadHandler>
    void async_read_until_buffersize(size_t size, const ReadHandler &handler);
    void async_process_response(http_response& response);
    void cancel_sending_response_with_error(const http_response& response, http::error_code& ec);
    void handle_headers_written(const http_response& response, const boost::system::error_code& ec);
    void handle_write_large_response(const http_response& response, const boost::system::error_code& ec);
    void handle_write_chunked_response(const http_response& response, const boost::system::error_code& ec);
    void handle_response_written(const http_response& response, const boost::system::error_code& ec);
    void finish_request_response();

    void handle_request_data_ready(size_t count, http_response& response)
    {
        if (count)
            async_process_response(response);
    }

    void handle_response(http_response& response, bool bad_request);
    void handle_body_buff_write(size_t count);
    void handle_chunked_body_buff_write(size_t count);
    void handle_chunked_response_buff_read(size_t count, uint8_t* membuf, http_response& response);
    void handle_large_response_buff_read(size_t count, http_response& response);
};

/// Custom HTTP request handler.
/// A Curiously recurring template pattern for creating custom HTTP request handlers.
class http_req_handler
{
public:
    http_req_handler();
    ~http_req_handler() {}

    /// Get the list of URLs paths for this handler.
    void url_path(std::set<std::string>& outlist)
    {
        url_func_(this, outlist);
    }

    void handle_request(http::http_request msg)
    {
        handler_func_(this, msg);
    }

protected:
    typedef void (*request_handler_func)(http_req_handler*, http::http_request);
    typedef void (*url_path_func)(http_req_handler*, std::set<std::string>&);

    http_req_handler(request_handler_func handler_func, url_path_func url_func)
        : url_func_(url_func), handler_func_(handler_func)
    {}

private:
    url_path_func url_func_;
    request_handler_func handler_func_;
};

/// A template class that servers as a implementation wrapper for a http_req_handler.
/// Template type Handler is the actual implementation for a http_req_handler (Curiously recurring template pattern).
template <typename Handler>
class http_req_handler_impl : public http_req_handler
{
public:

    /// Default constructor , (h) is the handler to be called for a HTTP request.
    http_req_handler_impl(Handler h)
      : http_req_handler(&http_req_handler_impl::handle_request_impl, &http_req_handler_impl::url_path_impl), handler_(h)
    {}

    static void handle_request_impl(http_req_handler* base, http::http_request msg)
    {
        http_req_handler_impl<Handler>* req_handler(static_cast<http_req_handler_impl<Handler>*>(base));
        req_handler->handler_.handle_request(msg);
    }

    static void url_path_impl(http_req_handler* base, std::set<std::string>& outlist)
    {
        http_req_handler_impl<Handler>* req_handler(static_cast<http_req_handler_impl<Handler>*>(base));
        req_handler->handler_.url_path(outlist);
    }

private:
    Handler handler_;
};

/// HTTP server class to track the connections, a tcp_listener implementation.
class http_listener
{
public:
    http_listener() {}
    typedef boost::shared_ptr<http_connection> http_conn_ptr;

    void do_accept(tcp_socket_ptr sock);

    void drop_connection(http_conn_ptr conn);

    std::set<http_conn_ptr> connections_;
};

/// HTTP service class.
class http_service
{
public:
    /// Factory for registering all the http_req_handler classes.
    typedef reg_factory<http_req_handler> req_handler_factory;

    http_service();
    virtual ~http_service() {}

    /// Entry point for every network service where a new connection is accepted and handled.
    void accept(tcp_socket_ptr sock);

    /// Get HTTP request handler object for registered to handle the given (url_path)
    /// If there are no handlers registered to handle this URL a NULL is returned.
    http_req_handler* get_req_handler(const std::string& url_path);

    static http_service* instance()
    {
        static http_service s_http_service;
        return &s_http_service;
    }

private:
    typedef boost::shared_ptr<http_req_handler> req_handler_ptr;

    // HTTP request handlers for every thread. ( thread id => ( URL path => handler ) )
    std::map<thread_id_t, std::map<std::string, req_handler_ptr>> handlers_;

    net_service_listener_factory<http_listener> listeners_factory_;
};

/// Wrapper class to register with the service factory
class http_service_factory_wrapper
{
public:
    /// Factory method.
    static net_service_base* create_object()
    {
        static net_service_impl<http_service> s_service_impl(*http_service::instance());
        return &s_service_impl;
    }
};

}}

#endif // HTTP_SERVICE_H_

