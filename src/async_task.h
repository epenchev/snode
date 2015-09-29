//
// async_task.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef _ASYNC_TASK_H_
#define _ASYNC_TASK_H_

#include "threadpool.h"
#include "snode_types.h"
#include "snode_core.h"

#include <functional>

namespace snode
{

/// Schedule an execution task to some worker thread specified with a thread id. Task is executed within the given worker context.
class async_event_task
{
public:

    template<typename F>
    static void inline connect(F func, thread_id_t id = THIS_THREAD_ID())
    {
        snode_core::instance().get_threadpool().schedule(func, id);
    }

    template <typename F, typename A1>
    static void inline connect(F func, A1 a1, thread_id_t id = THIS_THREAD_ID())
    {
        snode_core::instance().get_threadpool().schedule(std::bind(func, a1), id);
    }

    template <typename F, typename A1, typename A2>
    static void inline connect(F func, A1 a1, A2 a2, thread_id_t id = THIS_THREAD_ID())
    {
        snode_core::instance().get_threadpool().schedule(std::bind(func, a1, a2), id);
    }

    template <typename F, typename A1, typename A2, typename A3>
    static void inline connect(F func, A1 a1, A2 a2, A3 a3, thread_id_t id = THIS_THREAD_ID())
    {
        snode_core::instance().get_threadpool().schedule(std::bind(func, a1, a2, a3), id);
    }

    template <typename F, typename A1, typename A2, typename A3, typename A4>
    static void inline connect(F func, A1 a1, A2 a2, A3 a3, A4 a4, thread_id_t id = THIS_THREAD_ID())
    {
        snode_core::instance().get_threadpool().schedule(std::bind(func, a1, a2, a3, a4), id);
    }

    template <typename F, typename A1, typename A2, typename A3, typename A4, typename A5>
    static void inline connect(F func, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, thread_id_t id = THIS_THREAD_ID())
    {
        snode_core::instance().get_threadpool().schedule(std::bind(func, a1, a2, a3, a4, a5), id);
    }
};

}

#endif /* ASYNC_TASK_H_ */


