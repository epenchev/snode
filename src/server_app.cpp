//
// server_app.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria


#include "server_app.h"
#include <iostream>
using namespace boost::asio::ip;

namespace smkit
{

void server_app::run()
{
    this->_init();
    m_event_threadpool->run();
    // run main server thread, mostly responsible for logging and signal catching
    while (true)
    {
        try
        {
            auto task = m_task_queue.dequeue();
            task();
        } catch (...) {
            // Something bad happened log it
            // throw;
        }
    }

}

void server_app::_init()
{
    m_sysconfig.init("conf.xml");
    m_event_threadpool = new io_event_threadpool(m_sysconfig.io_threads());
    if (!m_sysconfig.server_handlers().size())
        throw std::logic_error("No handlers set in configuration");

    for (auto it = m_sysconfig.server_handlers().begin(); it != m_sysconfig.server_handlers().end(); it++)
    {
        tcp_acceptor_ptr sock_acceptor(new boost::asio::ip::tcp::acceptor(m_event_threadpool->service()));
        sock_acceptor->open(boost::asio::ip::tcp::v4());

        if (!it->host.empty())
        {
            boost::asio::ip::address ip_address;
            ip_address.from_string(it->host);
            sock_acceptor->bind(tcp::endpoint(ip_address, it->listen_port));
        }
        else
        {
            sock_acceptor->bind(tcp::endpoint(tcp::v4(), it->listen_port));
        }

        sock_acceptor->set_option(tcp::acceptor::reuse_address(true));
        sock_acceptor->listen();
        m_tcp_acceptors.push_back(sock_acceptor);

        // accept code follows
        tcp_socket_ptr socket(new tcp::socket(m_event_threadpool->service()));
        sock_acceptor->async_accept(*socket, boost::bind(&server_app::_handle_accept, this, socket, boost::asio::placeholders::error));

        // create a server handler for each thread
        for (unsigned i = 0; i < m_event_threadpool->threads().size(); i++)
        {
            server_handler* handler = server_handler_factory_t::create_instance(it->name);
            handler->init(it->listen_port, m_event_threadpool->threads().at(i)->get_id());
            m_server_handlers.push_back(handler);
        }
    }
}

void server_app::_handle_accept(tcp_socket_ptr socket, const error_code_t& err)
{
    if (!err)
    {
        error_code_t err_code;
        tcp::endpoint endpoint = socket->remote_endpoint(err_code);
        if (!err)
            std::cout << "[DEBUG] Connected from " << endpoint.port() << std::endl;
    }
}

sys_processor_threadpool& server_app::processor_threadpool()
{
    return *m_processor_threadpool;
}

io_event_threadpool& server_app::event_threadpool()
{
    return *m_event_threadpool;
}


}
