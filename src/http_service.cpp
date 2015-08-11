//
// http_service.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include <boost/type_traits.hpp>
#include <boost/algorithm/string/find.hpp>

#include "http_service.h"
#include "http_helpers.h"

using namespace boost::asio;
using namespace boost::asio::ip;

#define CRLF std::string("\r\n")

namespace snode
{
namespace http
{


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

#if 0
void hostport_listener::on_accept(ip::tcp::socket* socket, const boost::system::error_code& ec)
{
    if (ec)
    {
        delete socket;
    }
    else
    {
        {
            pplx::scoped_lock<pplx::extensibility::recursive_lock_t> lock(m_connections_lock);
            m_connections.insert(new connection(std::unique_ptr<tcp::socket>(std::move(socket)), m_p_server, this));
            m_all_connections_complete.reset();

            if (m_acceptor)
            {
                // spin off another async accept
                auto newSocket = new ip::tcp::socket(crossplat::threadpool::shared_instance().service());
                m_acceptor->async_accept(*newSocket, boost::bind(&hostport_listener::on_accept, this, newSocket, placeholders::error));
            }
        }
    }
}
#endif

#if 0
void hostport_listener::stop()
{
    // halt existing connections
    {
        pplx::scoped_lock<pplx::extensibility::recursive_lock_t> lock(m_connections_lock);
        m_acceptor.reset();
        for(auto connection : m_connections)
        {
            connection->close();
        }
    }

    m_all_connections_complete.wait();
}


void hostport_listener::add_listener(const std::string& path, web::http::experimental::listener::details::http_listener_impl* listener)
{
    pplx::extensibility::scoped_rw_lock_t lock(m_listeners_lock);

    if (!m_listeners.insert(std::map<std::string,web::http::experimental::listener::details::http_listener_impl*>::value_type(path, listener)).second)
        throw std::invalid_argument("Error: http_listener is already registered for this path");
}

void hostport_listener::remove_listener(const std::string& path, web::http::experimental::listener::details::http_listener_impl*)
{
    pplx::extensibility::scoped_rw_lock_t lock(m_listeners_lock);

    if (m_listeners.erase(path) != 1)
        throw std::invalid_argument("Error: no http_listener found for this path");
}
#endif


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
    request_.reply_if_not_already(status_codes::InternalError);
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
        else if (boost::iequals(http_verb, http::methods::TRACE))    http_verb = http::methods::TRACE;
        else if (boost::iequals(http_verb, http::methods::CONNECT)) http_verb = http::methods::CONNECT;
        else if (boost::iequals(http_verb, http::methods::OPTIONS)) http_verb = http::methods::OPTIONS;

        // Check to see if there is not allowed character on the input
        if (!validate_method(http_verb))
        {
            request_.reply(status_codes::BadRequest);
            close_ = true;
            do_response(true);
            return;
        }

        request_.set_method(http_verb);

        std::string http_path_and_version;
        std::getline(request_stream, http_path_and_version);
        const size_t VersionPortionSize = sizeof(" HTTP/1.1\r") - 1;

        // Make sure path and version is long enough to contain the HTTP version
        if(http_path_and_version.size() < VersionPortionSize + 2)
        {
            request_.reply(status_codes::BadRequest);
            close_ = true;
            do_response(true);
            return;
        }
#if 0
        // Get the path - remove the version portion and prefix space
        try
        {
            request_.set_request_uri(http_path_and_version.substr(1, http_path_and_version.size() - VersionPortionSize - 1));
        }
        catch(const uri_exception &e)
        {
            m_request.reply(status_codes::BadRequest, e.what());
            m_close = true;
            do_response(true);
            return;
        }
#endif

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
            request_.reply(status_codes::BadRequest);
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
        /* request_.get_impl()->complete(0, std::make_exception_ptr(http_exception(ec.value()))); */
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
        /* request_.get_impl()->complete(0, std::make_exception_ptr(http_exception(ec.value()))); */
    }
    else
    {
#if 0
        auto writebuf = request_.get_impl()->outstream().streambuf();
        writebuf.putn_nocopy(buffer_cast<const uint8_t *>(m_request_buf.data()), toWrite).then([=](pplx::task<size_t> writeChunkTask)
        {
            try
            {
                writeChunkTask.get();
            }
            catch (...)
            {
                request_.get_impl()->complete(0, std::current_exception());
                return;
            }

            request_buf_.consume(2 + toWrite);
            boost::asio::async_read_until(*socket_, request_buf_, CRLF,
                    boost::bind(&http_connection::handle_chunked_header, this, placeholders::error));
        });
#endif
    }
}

void http_connection::handle_body(const boost::system::error_code& ec)
{
    // read body
    if (ec)
    {
        /* request_.get_impl()->complete(0, std::make_exception_ptr(http_exception(ec.value()))); */
    }
    else if (read_ < read_size_)  // there is more to read
    {
        auto writebuf = request_.get_impl()->outstream().streambuf();
#if 0
        writebuf.putn_nocopy(boost::asio::buffer_cast<const uint8_t*>(request_buf_.data()), std::min(request_buf_.size(), read_size_ - read_)).then([=](pplx::task<size_t> writtenSizeTask)
        {
            size_t writtenSize = 0;
            try
            {
                writtenSize = writtenSizeTask.get();
            }
            catch (...)
            {
                request_.get_impl()->complete(0, std::current_exception());
                return;
            }
            read_ += writtenSize;
            request_buf_.consume(writtenSize);
            async_read_until_buffersize(std::min(ChunkSize, read_size_ - read_), boost::bind(&http_connection::handle_body, this, placeholders::error));
        });
#endif
    }
    else  // have read request body
    {
        request_.get_impl()->complete(read_);
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
#if 0
    // locate the listener:
    http_listener_impl* pListener = nullptr;
    {
        auto path_segments = uri::split_path(uri::decode(request_.relative_uri().path()));
        for (auto i = static_cast<long>(path_segments.size()); i >= 0; --i)
        {
            std::string path = "";
            for (size_t j = 0; j < static_cast<size_t>(i); ++j)
            {
                path += "/" + path_segments[j];
            }
            path += "/";

            pplx::extensibility::scoped_read_lock_t lock(m_p_parent->m_listeners_lock);
            auto it = m_p_parent->m_listeners.find(path);
            if (it != m_p_parent->m_listeners.end())
            {
                pListener = it->second;
                break;
            }
        }
    }

    if (pListener == nullptr)
    {
        request_.reply(status_codes::NotFound);
        do_response(false);
    }
    else
    {
        request_._set_listener_path(pListener->uri().path());
        do_response(false);

        // Look up the lock for the http_listener.
        pplx::extensibility::reader_writer_lock_t *pListenerLock;
        {
            pplx::extensibility::reader_writer_lock_t::scoped_lock_read lock(m_p_server->m_listeners_lock);

            // It is possible the listener could have unregistered.
            if(m_p_server->m_registered_listeners.find(pListener) == m_p_server->m_registered_listeners.end())
            {
                request_.reply(status_codes::NotFound);
                return;
            }
            pListenerLock = m_p_server->m_registered_listeners[pListener].get();

            // We need to acquire the listener's lock before releasing the registered listeners lock.
            // But we don't need to hold the registered listeners lock when calling into the user's code.
            pListenerLock->lock_read();
        }

        try
        {
            pListener->handle_request(request_);
            pListenerLock->unlock();
        }
        catch(...)
        {
            pListenerLock->unlock();
            request_._reply_if_not_already(status_codes::InternalError);
        }
    }
    
    if (--m_refs == 0) delete this;
#endif
}

void http_connection::do_response(bool bad_request)
{
#if 0
    ++m_refs;
    request_.get_response().then([=](pplx::task<http::http_response> r_task)
    {
        http::http_response response;
        try
        {
            response = r_task.get();
        }
        catch(...)
        {
            response = http::http_response(status_codes::InternalError);
        }

        // before sending response, the full incoming message need to be processed.
        if (bad_request)
        {
            async_process_response(response);
        }
        else
        {
            m_request.content_ready().then([=](pplx::task<http::http_request>)
            {
                async_process_response(response);
            });
        }
    });
#endif
}

void http_connection::async_process_response(http_response response)
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
            {
                close_ = true;
            }
        }
        os << header.first << ": " << header.second << CRLF;
    }
    os << CRLF;

    boost::asio::async_write(*socket_, response_buf_, boost::bind(&http_connection::handle_headers_written, this, response, placeholders::error));
}

void http_connection::cancel_sending_response_with_error(const http_response &response, const std::exception_ptr &eptr)
{
#if 0
    auto * context = static_cast<linux_request_context*>(response._get_server_context());
    context->m_response_completed.set_exception(eptr);
#endif
    // always terminate the connection since error happens
    finish_request_response();
}

void http_connection::handle_write_chunked_response(const http_response &response, const boost::system::error_code& ec)
{
    if (ec)
    {
        return handle_response_written(response, ec);
    }
        
    auto readbuf = response.get_impl()->instream().streambuf();
    if (readbuf.is_eof())
    {
        return cancel_sending_response_with_error(response, std::make_exception_ptr(http_exception("Response stream close early!")));
    }
    auto membuf = response_buf_.prepare(ChunkSize + snode::http::chunked_encoding::additional_encoding_space);

#if 0
    readbuf.getn(buffer_cast<uint8_t *>(membuf) + snode::http::chunked_encoding::additional_encoding_space, ChunkSize).then([=](pplx::task<size_t> actualSizeTask)
    {
        size_t actualSize = 0;
        try
        {
            actualSize = actualSizeTask.get();
        } catch (...)
        {
            return cancel_sending_response_with_error(response, std::current_exception());
        }

        size_t offset = http::details::chunked_encoding::add_chunked_delimiters(buffer_cast<uint8_t *>(membuf), ChunkSize + snode::http::chunked_encoding::additional_encoding_space, actualSize);
        response_buf_.commit(actualSize + snode::http::chunked_encoding::additional_encoding_space);
        response_buf_.consume(offset);
        boost::asio::async_write(
                *socket_,
                response_buf_,
                boost::bind(actualSize == 0 ? &http_connection::handle_response_written : &http_connection::handle_write_chunked_response,
                this,
                response, placeholders::error));
    });
#endif
}

void http_connection::handle_write_large_response(const http_response &response, const boost::system::error_code& ec)
{
    if (ec || write_ == write_size_)
        return handle_response_written(response, ec);

    auto readbuf = response.get_impl()->instream().streambuf();
    if (readbuf.is_eof())
        return cancel_sending_response_with_error(response, std::make_exception_ptr(http_exception("Response stream close early!")));
    size_t readBytes = std::min(ChunkSize, write_size_ - write_);

#if 0
    readbuf.getn(buffer_cast<uint8_t *>(response_buf.prepare_(readBytes)), readBytes).then([=](pplx::task<size_t> actualSizeTask)
    {
        size_t actualSize = 0;
        try
        {
            actualSize = actualSizeTask.get();
        } catch (...)
        {
            return cancel_sending_response_with_error(response, std::current_exception());
        }
        write_ += actualSize;
        response_buf_.commit(actualSize);
        boost::asio::async_write(*socket_, response_buf_, boost::bind(&http_connection::handle_write_large_response, this, response, placeholders::error));
    });
#endif
}

void http_connection::handle_headers_written(const http_response &response, const boost::system::error_code& ec)
{
    if (ec)
    {
        return cancel_sending_response_with_error(response, std::make_exception_ptr(http_exception(/*ec.value(),*/ "error writing headers")));
    }
    else
    {
        if (chunked_)
            handle_write_chunked_response(response, ec);
        else
            handle_write_large_response(response, ec);
    }
}

void http_connection::handle_response_written(const http_response &response, const boost::system::error_code& ec)
{
#if 0
    auto * context = static_cast<linux_request_context*>(response._get_server_context());
    if (ec)
    {
        return cancel_sending_response_with_error(response, std::make_exception_ptr(http_exception(ec.value(), "error writing response")));
    }
    else
    {
        context->m_response_completed.set();
        if (!m_close)
        {
            start_request_response();
        }
        else
        {
            finish_request_response();
        }
    }
#endif
}

void http_connection::finish_request_response()
{
#if 0
    // kill the connection
    {
        pplx::scoped_lock<pplx::extensibility::recursive_lock_t> lock(m_p_parent->m_connections_lock);
        m_p_parent->m_connections.erase(this);
        if (m_p_parent->m_connections.empty())
        {
            m_p_parent->m_all_connections_complete.set();
        }
    }
    
    close();
    if (--m_refs == 0) delete this;
#endif
}
/*
pplx::task<void> http_linux_server::start()
{
    pplx::extensibility::reader_writer_lock_t::scoped_lock_read lock(m_listeners_lock);

    auto it = m_listeners.begin();
    try
    {
        for (; it != m_listeners.end(); ++it)
        {
            it->second->start();
        }
    }
    catch (...)
    {
        while (it != m_listeners.begin())
        {
            --it;
            it->second->stop();
        }
        return pplx::task_from_exception<void>(std::current_exception());
    }

    m_started = true;
    return pplx::task_from_result();
}

pplx::task<void> http_linux_server::stop()
{
    pplx::extensibility::reader_writer_lock_t::scoped_lock_read lock(m_listeners_lock);

    m_started = false;

    for(auto & listener : m_listeners)
    {
        listener.second->stop();
    }

    return pplx::task_from_result();
}


std::pair<std::string,std::string> canonical_parts(const http::uri& uri)
{
    std::ostringstream endpoint;
    endpoint.imbue(std::locale::classic());
    endpoint << uri::decode(uri.host()) << ":" << uri.port();

    auto path = uri::decode(uri.path());

    if (path.size() > 1 && path[path.size()-1] != '/')
    {
        path += "/"; // ensure the end slash is present
    }

    return std::make_pair(endpoint.str(), path);
}

pplx::task<void> http_linux_server::register_listener(details::http_listener_impl* listener)
{
    auto parts = canonical_parts(listener->uri());
    auto hostport = parts.first;
    auto path = parts.second;

    {
        pplx::extensibility::scoped_rw_lock_t lock(m_listeners_lock);
        if (m_registered_listeners.find(listener) != m_registered_listeners.end())
        {
            throw std::invalid_argument("listener already registered");
        }

        try
        {
            m_registered_listeners[listener] = utility::details::make_unique<pplx::extensibility::reader_writer_lock_t>();

            auto found_hostport_listener = m_listeners.find(hostport);
            if (found_hostport_listener == m_listeners.end())
            {
                found_hostport_listener = m_listeners.insert(
                    std::make_pair(hostport, utility::details::make_unique<details::hostport_listener>(this, hostport))).first;

                if (m_started)
                {
                    found_hostport_listener->second->start();
                }
            }

            found_hostport_listener->second->add_listener(path, listener);
        }
        catch (...)
        {
            // Future improvement - really this API should entirely be asychronously.
            // the hostport_listener::start() method should be made to return a task
            // throwing the exception.
            m_registered_listeners.erase(listener);
            m_listeners.erase(hostport);
            throw;
        }
    }

    return pplx::task_from_result();
}

pplx::task<void> http_linux_server::unregister_listener(details::http_listener_impl* listener)
{
    auto parts = canonical_parts(listener->uri());
    auto hostport = parts.first;
    auto path = parts.second;
    // First remove the listener from hostport listener
    {
        pplx::extensibility::scoped_read_lock_t lock(m_listeners_lock);
        auto itr = m_listeners.find(hostport);
        if (itr == m_listeners.end())
        {
            throw std::invalid_argument("Error: no listener registered for that host");
        }

        itr->second->remove_listener(path, listener);
    }

    // Second remove the listener form listener collection
    std::unique_ptr<pplx::extensibility::reader_writer_lock_t> pListenerLock = nullptr;
    {
        pplx::extensibility::scoped_rw_lock_t lock(m_listeners_lock);
        pListenerLock = std::move(m_registered_listeners[listener]);
        m_registered_listeners[listener] = nullptr;
        m_registered_listeners.erase(listener);
    }

    // Then take the listener write lock to make sure there are no calls into the listener's
    // request handler.
    if (pListenerLock != nullptr)
    {
        pplx::extensibility::scoped_rw_lock_t lock(*pListenerLock);
    }

    return pplx::task_from_result();
}

pplx::task<void> http_linux_server::respond(http::http_response response)
{
    details::linux_request_context * p_context = static_cast<details::linux_request_context*>(response._get_server_context());
    return pplx::create_task(p_context->m_response_completed);
}
*/

}}
