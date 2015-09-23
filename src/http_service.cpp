//
// http_service.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include <memory>
#include <boost/type_traits.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/bind.hpp>

#include "http_service.h"
#include "http_helpers.h"
#include "http_msg.h"
#include "async_task.h"
#include "async_streams.h"
#include "uri_utils.h"
#include "snode_core.h"

using namespace boost::asio;
using namespace boost::asio::ip;

#define CRLF std::string("\r\n")

namespace snode
{
namespace http
{

// register http service into the global net_service factory
snode_core::service_factory::registrator<http_service> http_service_reg("http");

// Avoid using boost regex in the async_read_until() call.
// This class replaces the regex "\r\n\r\n|[\x00-\x1F]|[\x80-\xFF]"
// This is used as part of the async_read_until call below; see the
// following for more details:
// http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference/async_read_until/overload4.html
struct crlf_nonascii_searcher_t
{
    enum class State
    {
        none = 0,   // ".\r\n\r\n$"
        cr = 1,     // "\r.\n\r\n$"
                    // "  .\r\n\r\n$"
        crlf = 2,   // "\r\n.\r\n$"
                    // "    .\r\n\r\n$"
        crlfcr = 3  // "\r\n\r.\n$"
                    // "    \r.\n\r\n$"
                    // "      .\r\n\r\n$"
    };

    // This function implements the searcher which "consumes" a certain amount of the input
    // and returns whether or not there was a match (see above).

    // From the Boost documentation:
    // "The first member of the return value is an iterator marking one-past-the-end of the
    //  bytes that have been consumed by the match function. This iterator is used to
    //  calculate the begin parameter for any subsequent invocation of the match condition.
    //  The second member of the return value is true if a match has been found, false
    //  otherwise."
    template<typename Iter>
    std::pair<Iter, bool> operator()(const Iter begin, const Iter end) const
    {
        // In the case that we end inside a partially parsed match (like abcd\r\n\r),
        // we need to signal the matcher to give us the partial match back again (\r\n\r).
        // We use the excluded variable to keep track of this sequence point (abcd.\r\n\r
        // in the previous example).
        Iter excluded = begin;
        Iter cur = begin;
        State state = State::none;
        while (cur != end)
        {
            char c = *cur;
            if (c == '\r')
            {
                if (state == State::crlf)
                {
                    state = State::crlfcr;
                }
                else
                {
                    // In the case of State::cr or State::crlfcr, setting the state here
                    // "skips" a none state and therefore fails to move up the excluded
                    // counter.
                    excluded = cur;
                    state = State::cr;
                }
            }
            else if (c == '\n')
            {
                if (state == State::cr)
                {
                    state = State::crlf;
                }
                else if (state == State::crlfcr)
                {
                    ++cur;
                    return std::make_pair(cur, true);
                }
                else
                {
                    state = State::none;
                }
            }
            else if (c <= '\x1F' && c >= '\x00')
            {
                ++cur;
                return std::make_pair(cur, true);
            }
            else if (c <= '\xFF' && c >= '\x80')
            {
                ++cur;
                return std::make_pair(cur, true);
            }
            else
            {
                state = State::none;
            }
            ++cur;
            if (state == State::none)
                excluded = cur;
        }
        return std::make_pair(excluded, false);
    }
} crlf_nonascii_searcher;
}}

namespace boost
{
namespace asio
{
template <> struct is_match_condition<snode::http::crlf_nonascii_searcher_t> : public boost::true_type {};
}}


namespace snode
{
namespace http
{

const size_t ChunkSize = 4 * 1024;

http_service::http_service()
{
    const std::vector<thread_ptr>& threads = snode_core::instance().get_threadpool().threads();
    std::set<std::string> handlers_list;
    req_handler_factory::get_reg_list(handlers_list);

    // Setup request handlers for every thread
    for (auto name : handlers_list)
    {
        http_req_handler* req_handler = req_handler_factory::create_instance(name);
        std::set<std::string> paths;
        req_handler->url_path(paths);

        if (paths.empty())
            continue;

        for (auto thread_i : threads)
        {
            if (nullptr == req_handler)
            {
                req_handler = req_handler_factory::create_instance(name);
            }

            if (handlers_.count(thread_i->get_id()) > 0)
            {
                auto & handlers_map = handlers_[thread_i->get_id()];
                for (auto url_path : paths)
                {
                    // every URL path is handled from a unique handler
                    if (!handlers_map.count(url_path))
                    {
                        handlers_map[url_path] = req_handler_ptr(req_handler);
                    }
                }
            }
            else
            {
                std::map<std::string, req_handler_ptr> handlers_map;
                for (auto url_path : paths)
                {
                    handlers_map[url_path] = req_handler_ptr(req_handler);
                }

                handlers_[thread_i->get_id()] = handlers_map;
            }
            // create new request handler object for each thread
            req_handler = nullptr;
        }
    }
}

void http_service::accept(tcp_socket_ptr sock)
{
    auto listener = listeners_factory_.get_next_listener();
    listener->on_accept(sock);
}

http_req_handler* http_service::get_req_handler(const std::string& url_path)
{
    return NULL;
}

void http_listener::do_accept(tcp_socket_ptr sock)
{
    http_conn_ptr conn(new http_connection(sock, dynamic_cast<http_service*>(http_service::instance()), this));
    connections_.insert(conn);
}

void http_listener::drop_connection(http_conn_ptr conn)
{
    conn->close();
    connections_.erase(conn);
}

void http_connection::close()
{
    close_ = true;
    auto sock = socket_.get();
    if (sock != nullptr)
    {
        boost::system::error_code ec;
        sock->cancel(ec);
        sock->shutdown(tcp::socket::shutdown_both, ec);
        sock->close(ec);
    }
}

void http_connection::start_request_response()
{
    read_size_ = 0;
    read_ = 0;
    request_buf_.consume(request_buf_.size()); // clear the buffer

    // Wait for either double newline or a char which is not in the range [32-127] which suggests SSL handshaking.
    // For the SSL server support this line might need to be changed. Now, this prevents from hanging when SSL client tries to connect.
    async_read_until(*socket_, request_buf_, crlf_nonascii_searcher, boost::bind(&http_connection::handle_http_line, this, placeholders::error));
}

void http_connection::handle_http_line(const boost::system::error_code& ec)
{
    if (ec)
    {
        // client closed connection
        if (ec == boost::asio::error::eof || ec == boost::asio::error::operation_aborted)
        {
            finish_request_response();
        }
        else
        {
            request_.reply_if_not_already(status_codes::BadRequest);
            close_ = true;
            do_response(true);
        }
    }
    else
    {
        // read http status line
        std::istream request_stream(&request_buf_);
        request_stream.imbue(std::locale::classic());
        std::skipws(request_stream);

        std::string http_verb;
        request_stream >> http_verb;

        if (boost::iequals(http_verb, http::methods::GET))          http_verb = http::methods::GET;
        else if (boost::iequals(http_verb, http::methods::POST))    http_verb = http::methods::POST;
        else if (boost::iequals(http_verb, http::methods::PUT))     http_verb = http::methods::PUT;
        else if (boost::iequals(http_verb, http::methods::DEL))     http_verb = http::methods::DEL;
        else if (boost::iequals(http_verb, http::methods::HEAD))    http_verb = http::methods::HEAD;
        else if (boost::iequals(http_verb, http::methods::TRACE))   http_verb = http::methods::TRACE;
        else if (boost::iequals(http_verb, http::methods::CONNECT)) http_verb = http::methods::CONNECT;
        else if (boost::iequals(http_verb, http::methods::OPTIONS)) http_verb = http::methods::OPTIONS;

        // Check to see if there is not allowed character on the input
        if (!validate_method(http_verb))
        {
            request_.reply_if_not_already(status_codes::BadRequest);
            close_ = true;
            do_response(true);
            return;
        }

        request_.set_method(http_verb);

        std::string http_path_and_version;
        std::getline(request_stream, http_path_and_version);
        const size_t VersionPortionSize = sizeof(" HTTP/1.1\r") - 1;

        // Make sure path and version is long enough to contain the HTTP version
        if (http_path_and_version.size() < VersionPortionSize + 2)
        {
            request_.reply_if_not_already(status_codes::BadRequest);
            close_ = true;
            do_response(true);
            return;
        }

        // Get the path - remove the version portion and prefix space
        try
        {
            request_.set_request_url(http_path_and_version.substr(1, http_path_and_version.size() - VersionPortionSize - 1));
        }
        catch (const std::exception& ex)
        {
            request_.reply_if_not_already(status_codes::BadRequest);
            close_ = true;
            do_response(true);
            return;
        }


        // Get the version
        std::string http_version = http_path_and_version.substr(http_path_and_version.size() - VersionPortionSize + 1, VersionPortionSize - 2);
        // if HTTP version is 1.0 then disable pipelining
        if (http_version == "HTTP/1.0")
        {
            close_ = true;
        }

        handle_headers();
    }
}

void http_connection::handle_headers()
{
    std::istream request_stream(&request_buf_);
    request_stream.imbue(std::locale::classic());
    std::string header;
    while (std::getline(request_stream, header) && header != "\r")
    {
        auto colon = header.find(':');
        if (colon != std::string::npos && colon != 0)
        {
            auto name = header.substr(0, colon);
            auto value = header.substr(colon + 1, header.length() - (colon + 1)); // also exclude '\r'
            trim_whitespace(name);
            trim_whitespace(value);

            auto& currentValue = request_.headers()[name];
            if (currentValue.empty() || boost::iequals(name, header_names::content_length)) // (content-length is already set)
            {
                currentValue = value;
            }
            else
            {
                currentValue += ", " + value;
            }
        }
        else
        {
            request_.reply_if_not_already(status_codes::BadRequest);
            close_ = true;
            do_response(true);
            return;
        }
    }

    close_ = chunked_ = false;
    std::string name;
    // check if the client has requested we close the connection
    if (request_.headers().match(header_names::connection, name))
    {
        close_ = boost::iequals(name, "close");
    }

    if (request_.headers().match(header_names::transfer_encoding, name))
    {
        chunked_ = boost::ifind_first(name, "chunked");
    }

    snode::streams::producer_consumer_buffer<uint8_t> buf(512);
    request_.get_impl()->set_instream(buf.create_istream());
    request_.get_impl()->set_outstream(buf.create_ostream()/*, false*/);
    if (chunked_)
    {
        boost::asio::async_read_until(*socket_, request_buf_, CRLF, boost::bind(&http_connection::handle_chunked_header, this, placeholders::error));
        dispatch_request_to_listener();
        return;
    }

    if (!request_.headers().match(header_names::content_length, read_size_))
    {
        read_size_ = 0;
    }

    if (read_size_ == 0)
    {
        request_.get_impl()->complete(0);
    }
    else // need to read the sent data
    {
        read_ = 0;
        async_read_until_buffersize(std::min(ChunkSize, read_size_), boost::bind(&http_connection::handle_body, this, placeholders::error));
    }

    dispatch_request_to_listener();
}

void http_connection::handle_chunked_header(const boost::system::error_code& ec)
{
    if (ec)
    {
        request_.get_impl()->complete(0 /*,std::make_exception_ptr(http_exception(ec.value()))*/);
    }
    else
    {
        std::istream is(&request_buf_);
        is.imbue(std::locale::classic());
        int len;
        is >> std::hex >> len;
        request_buf_.consume(CRLF.size());
        read_ += len;
        if (len == 0)
            request_.get_impl()->complete(read_);
        else
            async_read_until_buffersize(len + 2, boost::bind(&http_connection::handle_chunked_body, this, boost::asio::placeholders::error, len));
    }
}

void http_connection::handle_chunked_body(const boost::system::error_code& ec, int toWrite)
{
    if (ec)
    {
        request_.get_impl()->complete(0 /*,std::make_exception_ptr(http_exception(ec.value()))*/);
    }
    else
    {
        auto writebuf = request_.get_impl()->outstream().streambuf();
        /* TODO use putn_nocopy */
        writebuf.putn(buffer_cast<const uint8_t*>(request_buf_.data()), toWrite, BIND_HANDLER(&http_connection::handle_chunked_body_buff_write));
    }
}

void http_connection::handle_chunked_body_buff_write(size_t count)
{
    if (count)
    {
        request_buf_.consume(2 + count); // clear the buffer
        boost::asio::async_read_until(*socket_, request_buf_, CRLF,
               boost::bind(&http_connection::handle_chunked_header, this, placeholders::error));
    }
    else
    {
        request_.get_impl()->complete(0 /*,std::current_exception()*/);
        return;
    }
}

void http_connection::handle_body(const boost::system::error_code& ec)
{
    // read body
    if (ec)
    {
        request_.get_impl()->complete(0 /* , std::make_exception_ptr(http_exception(ec.value())) */);
    }
    else if (read_ < read_size_)  // there is more to read
    {
        auto writebuf = request_.get_impl()->outstream().streambuf();
        /* TODO use putn_nocopy */
        writebuf.putn(buffer_cast<const uint8_t*>(request_buf_.data()), std::min(request_buf_.size(), read_size_ - read_), BIND_HANDLER(&http_connection::handle_body_buff_write));
    }
    else  // have read request body
    {
        request_.get_impl()->complete(read_);
    }
}

void http_connection::handle_body_buff_write(size_t count)
{
    if (count)
    {
        read_ += count;
        request_buf_.consume(count);
        async_read_until_buffersize(std::min(ChunkSize, read_size_ - read_), boost::bind(&http_connection::handle_body, this, placeholders::error));
    }
    else
    {
        request_.get_impl()->complete(0 /*,std::current_exception()*/);
        return;
    }
}

template <typename ReadHandler>
void http_connection::async_read_until_buffersize(size_t size, const ReadHandler &handler)
{
    auto bufsize = request_buf_.size();
    if (bufsize >= size)
        boost::asio::async_read(*socket_, request_buf_, transfer_at_least(0), handler);
    else
        boost::asio::async_read(*socket_, request_buf_, transfer_at_least(size - bufsize), handler);
}

void http_connection::dispatch_request_to_listener()
{
    // locate the listener:
    //http_listener_impl* pListener = nullptr;
    http_req_handler* p_handler = nullptr;
    {
        auto path_segments = uri::split_path(request_.request_url());
        for (auto i = static_cast<long>(path_segments.size()); i >= 0; --i)
        {
            std::string path = "";
            for (size_t j = 0; j < static_cast<size_t>(i); ++j)
            {
                path += "/" + path_segments[j];
            }
            path += "/";

            // locate handler
            p_handler = p_service_->get_req_handler(path);
        }
    }

    if (nullptr == p_handler)
    {
        request_.reply_if_not_already(status_codes::NotFound);
        do_response(false);
    }
    else
    {
        // wait for the response to become ready
        do_response(false);
        try
        {
            p_handler->handle_request(request_);
        }
        catch (...) // catch whatever we can
        {
            request_.reply_if_not_already(status_codes::InternalError);
        }
    }
}

void http_connection::do_response(bool bad_request)
{
    request_.get_response(BIND_HANDLER(&http_connection::handle_response, bad_request));
}

void http_connection::handle_response(http_response& response, bool bad_request)
{
    // before sending response, the full incoming message need to be processed.
    if (bad_request)
    {
        async_process_response(response);
    }
    else
    {
        if (request_.get_impl()->get_data_available())
            async_process_response(response);
        else
            request_.content_ready(BIND_HANDLER(&http_connection::handle_request_data_ready, response));
    }
}

void http_connection::async_process_response(http_response& response)
{
    response_buf_.consume(response_buf_.size()); // clear the buffer
    std::ostream os(&response_buf_);
    os.imbue(std::locale::classic());

    os << "HTTP/1.1 " << response.status_code() << " "
        << response.reason_phrase()
        << CRLF;

    chunked_ = false;
    write_ = write_size_ = 0;

    std::string transferencoding;
    if (response.headers().match(header_names::transfer_encoding, transferencoding) && transferencoding == "chunked")
    {
        chunked_  = true;
    }

    if (!response.headers().match(header_names::content_length, write_size_) && response.body())
    {
        chunked_ = true;
        response.headers()[header_names::transfer_encoding] = "chunked";
    }

    if (!response.body())
    {
        response.headers().add(header_names::content_length,0);
    }

    for(const auto & header : response.headers())
    {
        // check if the responder has requested we close the connection
        if (boost::iequals(header.first, "connection"))
        {
            if (boost::iequals(header.second, "close"))
                close_ = true;
        }
        os << header.first << ": " << header.second << CRLF;
    }
    os << CRLF;
    boost::asio::async_write(*socket_, response_buf_, boost::bind(&http_connection::handle_headers_written, this, response, placeholders::error));
}

void http_connection::cancel_sending_response_with_error(const http_response& response, http::error_code& ec)
{
    request_.get_impl()->response_send_complete(ec);
    // always terminate the connection since error happens
    finish_request_response();
}

void http_connection::handle_write_chunked_response(const http_response& response, const boost::system::error_code& ec)
{
    if (ec)
    {
        return handle_response_written(response, ec);
    }
        
    auto readbuf = response.get_impl()->instream().streambuf();
    if (readbuf.is_eof())
    {
        boost::system::error_code ec(boost::system::errc::make_error_code(boost::system::errc::io_error));
        http::error_code err(ec);
        return cancel_sending_response_with_error(response, err);
    }
    auto membuf = response_buf_.prepare(ChunkSize + snode::http::chunked_encoding::additional_encoding_space);

    readbuf.getn(buffer_cast<uint8_t *>(membuf) + snode::http::chunked_encoding::additional_encoding_space, ChunkSize,
                    BIND_HANDLER(&http_connection::handle_chunked_response_buff_read,
                                  buffer_cast<uint8_t*>(membuf), response));
}

void http_connection::handle_chunked_response_buff_read(size_t count, uint8_t* membuf, http_response& response)
{
    if (!count || !membuf)
    {
        boost::system::error_code ec(boost::system::errc::make_error_code(boost::system::errc::io_error));
        http::error_code err(ec);
        return cancel_sending_response_with_error(response, err);
    }
    else
    {
        size_t offset = http::chunked_encoding::add_chunked_delimiters(membuf, ChunkSize + snode::http::chunked_encoding::additional_encoding_space, count);
        response_buf_.commit(count + snode::http::chunked_encoding::additional_encoding_space);
        response_buf_.consume(offset);
        boost::asio::async_write(*socket_, response_buf_,
                 boost::bind(count == 0 ? &http_connection::handle_response_written : &http_connection::handle_write_chunked_response,
                               this, response, placeholders::error));
    }
}

void http_connection::handle_write_large_response(const http_response &response, const boost::system::error_code& ec)
{
    if (ec || (write_ == write_size_))
        return handle_response_written(response, ec);

    auto readbuf = response.get_impl()->instream().streambuf();
    if (readbuf.is_eof())
    {
        http::error_code err(boost::system::errc::make_error_code(boost::system::errc::io_error));
        cancel_sending_response_with_error(response, err);
        return;
    }
    size_t readBytes = std::min(ChunkSize, write_size_ - write_);

    auto membuf = response_buf_.prepare(readBytes);
    readbuf.getn(buffer_cast<uint8_t *>(membuf), readBytes, BIND_HANDLER(&http_connection::handle_large_response_buff_read, response));
}

void http_connection::handle_large_response_buff_read(size_t count, http_response& response)
{
    if (!count)
    {
        boost::system::error_code ec(boost::system::errc::make_error_code(boost::system::errc::io_error));
        http::error_code err(ec);
        return cancel_sending_response_with_error(response, err);
    }

    write_ += count;
    response_buf_.commit(count);
    boost::asio::async_write(*socket_, response_buf_, boost::bind(&http_connection::handle_write_large_response, this, response, placeholders::error));
}

void http_connection::handle_headers_written(const http_response& response, const boost::system::error_code& ec)
{
    if (ec)
    {
        boost::system::error_code ec(boost::system::errc::make_error_code(boost::system::errc::io_error));
        http::error_code err(ec);
        return cancel_sending_response_with_error(response, err);
    }
    else
    {
        if (chunked_)
            handle_write_chunked_response(response, ec);
        else
            handle_write_large_response(response, ec);
    }
}

void http_connection::handle_response_written(const http_response& response, const boost::system::error_code& ec)
{
    if (ec)
    {
        http::error_code err(ec);
        cancel_sending_response_with_error(response, err);
        return;
    }
    else
    {
        http::error_code err(ec);
        request_.get_impl()->response_send_complete(err);
        if (!close_)
        {
            start_request_response();
        }
        else
        {
            finish_request_response();
        }
    }
}

void http_connection::finish_request_response()
{
    close();
    p_listener_->drop_connection(shared_from_this());
}

}}
