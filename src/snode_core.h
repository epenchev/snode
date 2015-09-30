//
// snode_core.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef _SNODE_CORE_H_
#define _SNODE_CORE_H_

#include <set>
#include <list>
#include <string>
#include <memory>

#include <boost/asio.hpp>

#include "threadpool.h"
#include "reg_factory.h"
#include "config_reader.h"
#include "snode_types.h"
#include "net_service.h"

// postfix_in updated - smtpd_proxy clear allocated message blocks on I/O error, added more detailed information about I/O errors
namespace snode
{

/// Main system/core class, does complete system initialization and provides access to all core objects.
class snode_core
{
private:
    boost::asio::io_service                 ios;                 /// boost io_service object to perform all socket based I/O.
    snode::threadpool*                      threadpool_;         /// handling all I/O and event messaging tasks.
    std::list<tcp_acceptor_ptr>             acceptors_;          /// socket acceptors listening for incoming connections.
    snode_config                            config_;             /// global configuration.
    std::map<unsigned short, net_service_base*>  services_;           /// network port -> net_service object map association.

public:
    /// server_controller is a singleton, can be accessed only with this method.
    static snode_core& instance()
    {
        static snode_core s_core;
        return s_core;
    }

    /// Factory for registering all the server handler classes.
    typedef reg_factory<net_service_base> service_factory;

    void init();

    /// Main entry point of the system, read configuration and runs the system.
    void run();

    /// Stop server engine and cancel all tasks waiting to be executed.
    void stop();

    snode::threadpool& get_threadpool();

    snode::snode_config& get_config();

    /// get boost io_service object
    boost::asio::io_service& get_io_service();
private:
    /// internal initialization structure

    /// boost acceptor handler callback function
    void handle_accept(tcp_socket_ptr sock, tcp_acceptor_ptr acceptor, const boost::system::error_code& err);

    snode_core();
    ~snode_core();
};

}

#endif /* _SNODE_CORE_H_ */
