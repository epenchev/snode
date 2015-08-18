//
// net_service.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria


#include "async_task.h"
#include "net_service.h"

#include <iostream>
#include <utility>

using namespace boost::asio::ip;

namespace snode
{

void snode_core::run()
{
    this->init();
    ev_threadpool_->run();

    // run main server thread, mostly responsible for logging and signal catching
    while (true)
    {
        try
        {
            auto task = task_queue_.dequeue();
            task();
        }
        catch (std::exception& ex)
        {
            // Something bad happened log it
            // throw;
        }
    }
}

void snode_core::init()
{
    config_.init("conf.xml");
    ev_threadpool_ = new io_event_threadpool(config_.io_threads());
    if (!config_.net_services().size())
    {
        throw std::logic_error("No network services set in configuration");
    }

    for (auto it = config_.net_services().begin(); it != config_.net_services().end(); it++)
    {
        tcp_acceptor_ptr acceptor(new boost::asio::ip::tcp::acceptor(ev_threadpool_->io_service()));
        acceptor->open(boost::asio::ip::tcp::v4());

        if (!it->host.empty())
        {
            boost::asio::ip::address ip_address;
            ip_address.from_string(it->host);
            acceptor->bind(tcp::endpoint(ip_address, it->listen_port));
        }
        else
        {
            acceptor->bind(tcp::endpoint(tcp::v4(), it->listen_port));
        }

        acceptor->set_option(tcp::acceptor::reuse_address(true));
        acceptor->listen();
        acceptors_.push_back(acceptor);

        // accept code follows
        tcp_socket_ptr socket(new tcp::socket(acceptor->get_io_service()));
        acceptor->async_accept(*socket, boost::bind(&snode_core::accept_handler, this, socket, acceptor, boost::asio::placeholders::error));

        // create a network service object for every listening port
        services_[it->listen_port] = service_factory::create_instance(it->name);
    }
}

void snode_core::accept_handler(tcp_socket_ptr socket, tcp_acceptor_ptr acceptor, const error_code_t& err)
{
    if (!err)
    {
        try
        {
            unsigned netport = socket->local_endpoint().port();
            net_service* service = services_[netport];

            error_code_t err_code;
            tcp::endpoint endpoint = socket->remote_endpoint(err_code);
            if (!err)
            {
                if (!err_code)
                {
                    std::cout << "[DEBUG] Connected from " << endpoint.port() << std::endl;
                }
            }

            // listener service now is responsible for the connected socket
            async_event_task::connect(&net_service::handle_accept, service, socket);

            // continue accepting new connections
            tcp_socket_ptr sock(new tcp::socket(acceptor->get_io_service()));
            acceptor->async_accept(*sock, boost::bind(&snode_core::accept_handler, this, sock, acceptor, boost::asio::placeholders::error));
        }
        catch (std::exception& ex)
        {
            std::cout << "[ERROR] " << ex.what() << std::endl;
        }
    }
}

sys_processor_threadpool& snode_core::processor_threadpool()
{
    return *sys_threadpool_;
}

io_event_threadpool& snode_core::event_threadpool()
{
    return *ev_threadpool_;
}

net_service::net_service()
{
    init_listeners();
}

void net_service::handle_accept(tcp_socket_ptr socket)
{
    if (!listeners_.empty())
    {
        auto iter = listeners_.find(THIS_THREAD_ID());
        if (listeners_.end() != iter)
        {
            iter->second->handle_accept(socket);
        }
    }
}

void net_service::init_listeners()
{
    const std::vector<thread_ptr>& threads = snode_core::instance().event_threadpool().threads();
    auto thread_count = threads.size();

    // fill listeners with the thread id's and empty pointers so later real objects can be assigned
    for (unsigned idx = 0; idx < thread_count; idx++)
    {
        listeners_[threads[idx]->get_id()] = nullptr;
    }
}

} // snode
