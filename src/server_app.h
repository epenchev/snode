//
// server_app.h
// Copyright (C) 2015  Emil Penchev, Bulgaria


#ifndef SERVER_APP_H_
#define SERVER_APP_H_

#include <set>
#include <list>
#include <string>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include "threadpool.h"
#include "reg_factory.h"
#include "config_reader.h"

namespace smkit
{

typedef boost::system::error_code error_code_t;
typedef boost::shared_ptr<boost::asio::ip::tcp::socket> tcp_socket_ptr;
typedef boost::shared_ptr<boost::asio::ip::tcp::acceptor> tcp_acceptor_ptr;

class server_handler
{
public:
    virtual ~server_handler() {}

    /// Factory creator method, to be overridden by subclasses.
    static server_handler* create_object() { return NULL; }
    virtual void init(unsigned short port, thread_id_t id) = 0;
    virtual unsigned short port() = 0;
    virtual thread_id_t thread_id() = 0;

    /// Custom hook method for accepting incoming TCP connections, implementation is in subclasses.
    /// Custom server_handlers receive a connected TCP socket object ready for I/O.
    virtual void accept_connection(tcp_socket_ptr socket) = 0;
};


/// Main system class, does complete system initialization and provides access to all core objects.
class server_app
{
private:
    std::vector<server_handler*>            m_server_handlers;      /// Protocol handlers
    io_event_threadpool*                    m_event_threadpool;     /// handling all I/O and event messaging tasks.
    sys_processor_threadpool*               m_processor_threadpool; /// handling all heavy computing tasks.
    std::list<tcp_acceptor_ptr>             m_tcp_acceptors;        /// socket acceptors listening for incoming connections.
    smkit_config                            m_sysconfig;            /// master configuration.
    sys_processor_threadpool::task_queue_t  m_task_queue;

public:
    /// server_controller is a singleton, can be accessed only with this method.
    static server_app& instance()
    {
        static server_app s_app;
        return s_app;
    }

    /// Factory for registering all the server handler classes.
    typedef reg_factory<server_handler> server_handler_factory_t;

    /// Main entry point of the system, read configuration and runs the system.
    void run();
    sys_processor_threadpool& processor_threadpool();
    io_event_threadpool& event_threadpool();

private:
    /// internal initialization structure
    void _init();

    /// boost acceptor handler callback function
    void _handle_accept(tcp_socket_ptr socket, const error_code_t& err);

    server_app() : m_event_threadpool(NULL), m_processor_threadpool(NULL)
    {
    }

    ~server_app()
    {
        delete m_event_threadpool;
    }
};

}

#endif /* SERVER_APP_H_ */
