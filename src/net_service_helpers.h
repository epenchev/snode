//
// net_service_helpers.h
// Copyright (C) 2015  Emil Penchev, Bulgaria


#ifndef _NET_SERVICE_HELPERS_H_
#define _NET_SERVICE_HELPERS_H_

#include <boost/asio.hpp>
#include <memory>
#include <map>

#include "thread_wrapper.h"
#include "snode_core.h"
#include "snode_types.h"

namespace snode
{
static const char* s_listener_factory_msg = "invalid thread id";

/// Helper class for processing accepted socket connections from a net_service object.
/// Completely independent from the net_service, helps you define a custom strategy
/// for accepting and forwarding connections to a specific thread worker.
class net_service_listener_base
{
public:
    void on_accept(tcp_socket_ptr sock) { func_(this, sock); }

    /// Get associated thread id with the listener
    const thread_id_t& thread_id() { return thread_id_; }
protected:
    typedef void (*on_accept_func)(net_service_listener_base*, tcp_socket_ptr);
    net_service_listener_base(on_accept_func func, thread_id_t id) : thread_id_(id), func_(func)
    {}

private:
    thread_id_t thread_id_;
    on_accept_func func_;
};

typedef std::shared_ptr<net_service_listener_base> net_service_listener_base_ptr;

/// (Listener) net_service_listener implementation.
/// Listener implementation must have do_accept(tcp_socket_ptr) method implemented and to be copy constructive.
template <typename Listener>
class net_service_listener : public net_service_listener_base
{
public:
    net_service_listener(Listener impl, thread_id_t id)
     : net_service_listener_base(&net_service_listener::on_accept_impl, id), impl_(impl)
    {}

    static void on_accept_impl(net_service_listener_base* base, tcp_socket_ptr sock)
    {
        net_service_listener<Listener>* listener(static_cast<net_service_listener<Listener>*>(base));
        listener->impl_.do_accept(sock);
    }

private:
    Listener impl_;
};

/// Helper factory to ease the listener => thread_id mapping and object construction.
/// Creates thread workers for a net_service.
/// ServiceListener is the template argument for creating the listener objects.
template <typename ServiceListener>
class net_service_listener_factory
{
public:
    net_service_listener_factory()
     : last_used_(-1)
    {
        auto threads = snode_core::instance().get_threadpool().threads();
        auto thread_count = threads.size();
        // map every listener with a worker thread id
        for (int idx = 0; idx < thread_count; idx++)
        {
            ServiceListener impl;
            listeners_[idx] = std::make_shared<net_service_listener<ServiceListener>>(impl, threads[idx]->get_id());
        }
    }

    /// Get listener for a specific thread_id (id).
    /// If there is no match for this id a runtime error exception is thrown
    net_service_listener_base_ptr get_listener(thread_id_t id)
    {
        auto threads = snode_core::instance().get_threadpool().threads();
        auto thread_count = threads.size();

        for (int idx = 0; idx < thread_count; idx++)
        {
            if (threads[idx]->get_id() == id)
            {
                last_used_ = idx;
                return listeners_[idx];
            }
        }
        // no thread id found with the given id
        throw std::runtime_error(s_listener_factory_msg);
    }

    /// Round robin mechanism for fetching a net_service_listener.
    net_service_listener_base_ptr get_next_listener()
    {
        if (last_used_ < 0)
        {
            last_used_ = 0;
        }
        else
        {
            auto iter = listeners_.find(last_used_);
            if ((++iter) != listeners_.end())
            {
                last_used_ = iter->first;
            }
            else
            {
                last_used_ = listeners_.begin()->first;
            }
        }
        return listeners_[last_used_];
    }

protected:
    int last_used_;
    std::map<int, net_service_listener_base_ptr> listeners_;
};

}

#endif /* _NET_SERVICE_HELPERS_H_ */
