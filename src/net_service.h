//
// net_service.h
// Copyright (C) 2015  Emil Penchev, Bulgaria


#ifndef _NET_SERVICE_H_
#define _NET_SERVICE_H_

#include "snode_types.h"

namespace snode
{

/// Represents a network service abstraction layer
/// handling incoming TCP connections via socket (ex. HTTP, HTTPS, RTMP ..).
class net_service_base
{
public:
    /// Entry point for a network service where a new TCP socket is accepted from the core system and handled as new connection.
    void accept(tcp_socket_ptr sock)
    {
        func_(this, sock);
    }

    /// Factory method.
    /// objects from this class will not be created directly but from a reg_factory<> instance.
    static net_service_base* create_object() { return NULL; }

protected:

    typedef void (*accept_func)(net_service_base*, tcp_socket_ptr);
    net_service_base(accept_func func) : func_(func)
    {}

    accept_func func_;
};

/// A Curiously recurring template pattern for creating custom net service objects.
/// ServiceImpl template is the actual service implementation.
/// A custom implementation must implement an accept() method and an factory class that complies with reg_factory.
template<typename ServiceImpl>
class net_service_impl : public net_service_base
{
public:
    static void handle_accept(net_service_base* base, tcp_socket_ptr sock)
    {
        net_service_impl<ServiceImpl>* service(static_cast<net_service_impl<ServiceImpl>*>(base));
        service->impl_.accept(sock);
    }

    net_service_impl(ServiceImpl& impl) : net_service_base(&net_service_impl::handle_accept), impl_(impl)
    {}
private:
    ServiceImpl& impl_;
};

} // end namespace snode

#endif /* _NET_SERVICE_H_ */
