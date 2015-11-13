//
// snode_core.cpp
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
    if (!threadpool_)
        throw std::runtime_error("Must call init() first");

    try
    {
        // main process thread is the only one doing I/O
        ios.run();
    }
    catch (std::exception& ex)
    {
        // report the error
    }
}

void snode_core::stop()
{
    threadpool_->stop();
    for (auto acceptor_it : acceptors_)
    {
        boost::system::error_code err;
        acceptor_it->cancel(err);
    }
    acceptors_.clear();

    ios.stop();
    ios.reset();
}

void snode_core::init(const std::string& filepath)
{
    if (!filepath.empty())
        config_.init(filepath);
    else
        throw std::runtime_error("missing XML configuration file");

    if (config_.error())
        return;

    threadpool_ = new threadpool(config_.threads());
    for (auto it = config_.services().begin(); it != config_.services().end(); it++)
    {
        net_service_base* service = service_factory::create_instance(it->name);
        if (!service)
        {
            continue;
        }

        tcp_acceptor_ptr acceptor(new boost::asio::ip::tcp::acceptor(ios));
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
        acceptor->async_accept(*socket, boost::bind(&snode_core::handle_accept, this, socket, acceptor, boost::asio::placeholders::error));

        // create a network service object for every listening port
        services_[it->listen_port] = service;
    }
}

void snode_core::handle_accept(tcp_socket_ptr sock, tcp_acceptor_ptr acceptor, const boost::system::error_code& err)
{
    if (!err)
    {
        try
        {
            // service is responsible to dispatch this to a worker thread
            net_service_base* service = services_[acceptor->local_endpoint().port()];
            service->accept(sock);

            // continue accepting new connections
            tcp_socket_ptr newsock(new tcp::socket(acceptor->get_io_service()));
            acceptor->async_accept(*newsock, boost::bind(&snode_core::handle_accept, this, newsock, acceptor, boost::asio::placeholders::error));
        }
        catch (std::exception& ex)
        {
            sock->close();
        }
    }
}

snode_core::snode_core() : threadpool_(nullptr)
{}

snode_core::~snode_core()
{
    delete threadpool_;
}

threadpool& snode_core::get_threadpool()
{
    return *threadpool_;
}

boost::asio::io_service& snode_core::get_io_service()
{
    return ios;
}

snode::snode_config& snode_core::get_config()
{
    return config_;
}

} // snode



