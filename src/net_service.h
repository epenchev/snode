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
class net_service
{
public:
    net_service() {}
    virtual ~net_service() {}

    /// Entry point for a network service where a new TCP socket is accepted from the core system and handled as new connection.
    virtual void accept(tcp_socket_ptr sock) {}

    /// Factory method.
    /// It's very important for subclasses to overload this method,
    /// objects from this class will not be created directly but from a reg_factory<> instance.
    static net_service* create_object() { return NULL; }
};

}

#endif /* _NET_SERVICE_H_ */
