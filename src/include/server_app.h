//
// server_app.h
// Copyright (C) 2015  Emil Penchev, Bulgaria


#ifndef SERVER_APP_H_
#define SERVER_APP_H_

#include <stream.h>
#include <task.h>
#include <reg_factory.h>
#include <xml_reader.h>

#include <set>
#include <list>
#include <string>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>

namespace smkit
{

typedef boost::system::error_code error_code_t;
typedef boost::thread::id thread_id_t;
typedef boost::shared_ptr<boost::asio::ip::tcp::socket> tcp_socket_ptr;

class stream; // forward

/// Accept incoming TCP connections and notify listeners
class socket_acceptor
{
public:
    /// Listener to be notified for every incoming TCP connection by SocketAcceptor instance. */
    class listener
    {
    public:
        /// Triggered when a new socket connection is accepted.
        /// Listener object receives connected TCP socket object and a thread id used for scheduling async tasks and I/O.
        virtual void on_accept(tcp_socket_ptr socket, thread_id_t tid) = 0;
    };

    socket_acceptor(unsigned short port, boost::asio::io_service& io_service, socket_acceptor::listener* listener)
     : m_port(port), m_server(listener), m_acceptor(io_service)
    {
        m_host.clear();
    }

    socket_acceptor(std::string host, unsigned short port, boost::asio::io_service& io_service,
                    socket_acceptor::listener* listener)
     : m_port(port), m_host(host), m_server(listener), m_acceptor(io_service)
    {
    }

    ~socket_acceptor()
    {
        stop();
    }

    /// Start accepting new connection sockets.
    void start();
    /// Close acceptor and cancel accepting incoming connections.
    void stop();

private:
    unsigned short                 m_port;
    std::string                    m_host;
    socket_acceptor::listener*     m_server;
    boost::asio::ip::tcp::acceptor m_acceptor;

    /// Accepts incoming connections.
    void accept();

    /// boost acceptor handler callback function
    void handle_accept(boost::asio::ip::tcp::socket* socket, const boost::system::error_code& err, thread_id_t tid);
};


/// Main system class, does complete system initialization and provides access to all core objects.
class server_app : public socket_acceptor::listener
{
public:
    /// server_controller is a singleton, can be accessed only with this method.
    static server_app& instance()
    {
        static server_app s_app;
        return s_app;
    }

    class server_handler
    {
    public:
        virtual ~server_handler() {}

        /// Factory creator method, to be overridden by subclasses.
        static server_handler* create_object() { return NULL; }

        /// Custom hook method for accepting incoming TCP connections, implementation is in subclasses.
        /// Custom server_handlers receive a connected TCP socket object ready for I/O.
        virtual void accept_connection(tcp_socket_ptr socket) = 0;
    };

    /// Factory for registering all the server handler classes.
    typedef reg_factory<server_handler> server_handler_factory_t;

    /// Main entry point of the system, read configuration from a file and runs the system.
    void init(const std::string& filename);

    /// run the smkit server
    void run();

    /// From socket_acceptor::listener.
    void on_accept(tcp_socket_ptr socket, thread_id_t tid);

    /// Get a player for a given stream,
    /// returns NULL if no stream is registered with this name.
    smkit::player* get_player(const std::string& stream);

private:
    smkit::xml_reader                               m_config_reader;
    boost::asio::io_service                         m_io_service;    /// main process io_service, all socket acceptors are attached on.
    std::list<socket_acceptor*>                     m_acceptors;     /// socket acceptors listening for incoming connections.
    std::map<std::string, smkit::stream_config*>    m_streams;       /// all media streams (name -> stream)

    /// server handlers to process every TCP connection
    std::list<std::pair<unsigned int, server_handler*> > m_srv_handlers;

    /// server handlers are associated with each thread, thread_id -> map(port, handler)
    std::map<task_scheduler::thread_id_t, std::map<unsigned int, server_handler*> > m_srv_handlers_assoc;

    void init();
    void load_config(const std::string& filename);

    server_app();
    ~server_app()
    {
    }
};

} // end namespace smkit

#endif /* SERVER_APP_H_ */
