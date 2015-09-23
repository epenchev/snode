//
// async_task.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef ASYNC_TASK_H_
#define ASYNC_TASK_H_

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <vector>

#include "net_service.h"
#include "seq_generator.h"
#include "threadpool.h"
#include "snode_types.h"
#include "snode_core.h"

namespace snode
{
/// Schedule an execution task for I/O event processing. Task is executed in a given I/O event loop thread specified by id.
/// Don't schedule any heavy processing tasks that require lots of CPU time because I/O may be delayed, use async_processing_task instead.
class async_event_task
{
public:

    template<typename Func>
    static void connect(Func func, thread_id_t id = THIS_THREAD_ID())
    {
        snode_core::instance().get_threadpool().schedule(boost::bind(func), id);
    }

    template <typename Func, typename Param1>
    static void connect(Func func, Param1 param1, thread_id_t id = THIS_THREAD_ID())
    {
        snode_core::instance().get_threadpool().schedule(boost::bind(func, param1), id);
    }

    template <typename Func, typename Param1, typename Param2>
    static void connect(Func func, Param1 param1, Param2 param2, thread_id_t id = THIS_THREAD_ID())
    {
        snode_core::instance().get_threadpool().schedule(boost::bind(func, param1, param2), id);
    }

    template <typename Func, typename Param1, typename Param2, typename Param3>
    static void connect(Func func, Param1 param1, Param2 param2, Param3 param3, thread_id_t id = THIS_THREAD_ID())
    {
        snode_core::instance().get_threadpool().schedule(boost::bind(func, param1, param2, param3), id);
    }
};

class async_event_timer
{
public:
    async_event_timer()
      : timerid_(sequence_id_generator::instance().next())
    {
    }

    ~async_event_timer()
    {
    }

    template<typename Func>
    void schedule(Func func, unsigned millisec, thread_id_t id = THIS_THREAD_ID())
    {
        create_timer(boost::bind(func), millisec, id);
    }

    template<typename Func, typename Param1>
    void schedule(Func func, Param1 param1, int millisec, thread_id_t id = THIS_THREAD_ID())
    {
        create_timer(boost::bind(func, param1), millisec, id);
    }

    template<typename Func, typename Param1, typename Param2>
    void schedule(Func func, Param1 param1, Param2 param2, int millisec, thread_id_t id = THIS_THREAD_ID())
    {
        create_timer(boost::bind(func, param1, param2), millisec, id);
    }

    template<typename Func, typename Param1, typename Param2, typename Param3>
    void schedule(Func func, Param1 param1, Param2 param2, Param3 param3, int millisec, thread_id_t id = THIS_THREAD_ID())
    {
        create_timer(boost::bind(func, param1, param2, param3), millisec, id);
    }

    void clear()
    {
        boost::system::error_code err;
        // error is not checked, just prevent exception to be thrown.
        timer_->cancel(err);
    }

private:
    typedef unsigned int                                   timer_id_t;
    typedef boost::shared_ptr<boost::asio::deadline_timer> timer_ptr;

    template<typename Task>
    void create_timer(Task taskfunc, unsigned millisec, thread_id_t id)
    {
        boost::system::error_code err;
        try
        {
            // timer_ = timer_ptr(new boost::asio::deadline_timer(snode_core::instance().get_threadpool().service(id)));
            // timer_->expires_from_now(boost::posix_time::milliseconds(millisec), err);
            if (!err)
            {
                timer_->async_wait(taskfunc);
            }
        }
        catch (const std::exception& err)
        {
            // do nothing just don't throw up :)
        }
    }

    timer_id_t timerid_;
    timer_ptr  timer_;
};

}

#endif /* ASYNC_TASK_H_ */


