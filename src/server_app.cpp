//
// server_app.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria


#include "server_app.h"
#include "async_task.h"

#include <iostream>
#include <utility>

using namespace boost::asio::ip;

namespace snode
{

void server_app::run()
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
            std::cout << "[ERROR] " << ex.what() << std::endl;
            // throw;
        }
    }
}

void server_app::init()
{
    config_.init("conf.xml");
    ev_threadpool_ = new io_event_threadpool(config_.io_threads());
    if (!config_.servers().size())
        throw std::logic_error("No handlers set in configuration");

    for (auto it = config_.servers().begin(); it != config_.servers().end(); it++)
    {
        tcp_acceptor_ptr acceptor(new boost::asio::ip::tcp::acceptor(ev_threadpool_->service()));
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
        acceptor->async_accept(*socket, boost::bind(&server_app::accept_handler, this, socket, acceptor, boost::asio::placeholders::error));

        // create a server handler for each thread
        for (unsigned idx = 0; idx < ev_threadpool_->threads().size(); idx++)
        {
            // create a TCP listener service for every thread
            tcp_listener* listener = service_factory::create_instance(it->name);

            thread_id_t tid = ev_threadpool_->threads().at(idx)->get_id();
            listeners_[tid][it->listen_port] = std::make_pair(listener, it->name);
        }
    }
}

void server_app::accept_handler(tcp_socket_ptr socket, tcp_acceptor_ptr acceptor, const error_code_t& err)
{
    if (!err)
    {
        try
        {
            unsigned netport = socket->local_endpoint().port();
            thread_id_t tid = ev_threadpool_->threads().at(current_thread_idx_)->get_id();
            tcp_listener* listener = listeners_[tid][netport].first;

            error_code_t err_code;
            tcp::endpoint endpoint = socket->remote_endpoint(err_code);
            if (!err)
            {
                std::cout << "[DEBUG] Connected from " << endpoint.port() << std::endl;
            }

            // listener service now is responsible for the connected socket
            async_event_task::connect(&tcp_listener::handle_accept, listener, socket);

            // continue accepting new connections
            tcp_socket_ptr waitsock(new tcp::socket(acceptor->get_io_service()));
            acceptor->async_accept(*waitsock, boost::bind(&server_app::accept_handler, this, waitsock, acceptor, boost::asio::placeholders::error));
        }
        catch (std::exception& ex)
        {
            std::cout << "[ERROR] " << ex.what() << std::endl;
        }
    }
}

sys_processor_threadpool& server_app::processor_threadpool()
{
    return *sys_threadpool_;
}

io_event_threadpool& server_app::event_threadpool()
{
    return *ev_threadpool_;
}


}
