//
// handler_allocator.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef _HANDLER_ALLOCATOR_H_
#define _HANDLER_ALLOCATOR_H_

#include <boost/aligned_storage.hpp>

#include "snode_types.h"
#include "async_task.h"

namespace snode
{

static const int alloc_size = 1024;

/// Class to manage the memory to be used for custom allocation of ASIO handler objects .
/// It contains a single block of memory which may be returned for allocation
/// requests. If the memory is in use when an allocation request is made, the
/// allocator delegates allocation to the global heap.
class handler_allocator
{
public:
    handler_allocator() : in_use_(false)
    {}

    void* allocate(std::size_t size)
    {
        if (!in_use_ && size < storage_.size)
        {
            in_use_ = true;
            return storage_.address();
        }
        else
        {
            return ::operator new(size);
        }
    }

    void deallocate(void* pointer)
    {
        if (pointer == storage_.address())
        {
            in_use_ = false;
        }
        else
        {
            ::operator delete(pointer);
        }
    }

private:
    // disable copy
    handler_allocator(const handler_allocator&);
    void operator=(const handler_allocator&);

    // Storage space used for handler-based custom memory allocation.
    boost::aligned_storage<alloc_size> storage_;

    // Whether the handler-based custom allocation storage has been used.
    bool in_use_;
};


/// Wrapper class template for handler objects to allow handler memory
/// allocation to be customized and handler to be dispatched to a given worker thread.
/// Calls to operator() are forwarded to the encapsulated handler.
/// A custom allocator strategy can be specified via the Allocator template parameter.
template <typename Handler, typename Allocator>
class asio_handler_dispatcher
{
public:
    asio_handler_dispatcher(Handler h, Allocator& a, thread_id_t id)
    :  handler_(h), allocator_(a), thread_id_(id)
    {
    }

    template <typename Arg1>
    void operator()(Arg1 arg1)
    {
        async_task::connect(handler_, arg1, thread_id_);
    }

    template <typename Arg1, typename Arg2>
    void operator()(Arg1 arg1, Arg2 arg2)
    {
        async_task::connect(handler_, arg1, arg2, thread_id_);
    }

    template <typename Arg1, typename Arg2, typename Arg3>
    void operator()(Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        async_task::connect(handler_, arg1, arg2, arg3, thread_id_);
    }

    template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    void operator()(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        async_task::connect(handler_, arg1, arg2, arg3, arg4, thread_id_);
    }

    template <typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
    void operator()(Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
    {
        async_task::connect(handler_, arg1, arg2, arg3, arg4, arg5, thread_id_);
    }

    /// Asio hook for handler allocation
    friend void* asio_handler_allocate(std::size_t size,
                                       asio_handler_dispatcher<Handler, Allocator>* this_handler)
    {
        return this_handler->allocator_.allocate(size);
    }

    /// Asio hook for handler deallocation
    friend void asio_handler_deallocate(void* pointer, std::size_t /*size*/,
                                        asio_handler_dispatcher<Handler, Allocator>* this_handler)
    {
        this_handler->allocator_.deallocate(pointer);
    }

private:
    Handler handler_;
    Allocator& allocator_;

    // id of the current worker thread
    thread_id_t thread_id_;

};

/// Helper function to wrap a handler object to add custom allocation.
/// A custom allocator strategy can be specified via the Allocator template parameter,
/// otherwise default strategy will be used.
template <typename Handler,
          typename Allocator = snode::handler_allocator>
inline asio_handler_dispatcher<Handler, Allocator>
make_alloc_handler(Handler h, Allocator& a, thread_id_t id)
{
    return asio_handler_dispatcher<Handler, Allocator>(h, a, id);
}

}
#endif /* _HANDLER_ALLOCATOR_H_ */
