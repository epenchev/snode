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

#include "server_app.h"
#include "seq_generator.h"

namespace snode
{
/// Schedule an execution task for I/O event processing. Task is executed in a given I/O event loop thread specified by id.
/// Don't schedule any heavy processing tasks that require lots of CPU time because I/O may be delayed, use async_processing_task instead.
class async_event_task
{
public:

    template<typename _Func>
    static void connect(_Func func, thread_id_t id = _THIS_THREAD_ID())
    {
        server_app::instance().event_threadpool().schedule(boost::bind(func), id);
    }

    template <typename _Func, typename _Param1>
    static void connect(_Func func, _Param1 param1, thread_id_t id = _THIS_THREAD_ID())
    {
        server_app::instance().event_threadpool().schedule(boost::bind(func, param1), id);
    }

    template <typename _Func, typename _Param1, typename _Param2>
    static void connect(_Func func, _Param1 param1, _Param2 param2, thread_id_t id = _THIS_THREAD_ID())
    {
        server_app::instance().event_threadpool().schedule(boost::bind(func, param1, param2), id);
    }

    template <typename _Func, typename _Param1, typename _Param2, typename _Param3>
    static void connect(_Func func, _Param1 param1, _Param2 param2, _Param3 param3, thread_id_t id = _THIS_THREAD_ID())
    {
        server_app::instance().event_threadpool().schedule(boost::bind(func, param1, param2, param3), id);
    }
};

/// Schedule an execution task for general processing. Task is executed in a given thread loop specified by id.
/// Those type of task a used mostly for heavy computation routines which require lots of CPU time.
class async_processing_task
{
public:

    template<typename _Func>
    static void connect(_Func func, thread_id_t id = _THIS_THREAD_ID())
    {
        server_app::instance().processor_threadpool().schedule(boost::bind(func), id);
    }

    template <typename _Func, typename _Param1>
    static void connect(_Func func, _Param1 param1, thread_id_t id = _THIS_THREAD_ID())
    {
        server_app::instance().processor_threadpool().schedule(boost::bind(func, param1), id);
    }

    template <typename _Func, typename _Param1, typename _Param2>
    static void connect(_Func func, _Param1 param1, _Param2 param2, thread_id_t id = _THIS_THREAD_ID())
    {
        server_app::instance().processor_threadpool().schedule(boost::bind(func, param1, param2), id);
    }

    template <typename _Func, typename _Param1, typename _Param2, typename _Param3>
    static void connect(_Func func, _Param1 param1, _Param2 param2, _Param3 param3, thread_id_t id = _THIS_THREAD_ID())
    {
        server_app::instance().processor_threadpool().schedule(boost::bind(func, param1, param2, param3), id);
    }
};

class async_event_timer
{
public:
    async_event_timer()
      : m_timerid(sequence_id_generator::instance().next())
    {
    }

    ~async_event_timer()
    {
    }

    template<typename _Func>
    void schedule(_Func func, unsigned millisec, thread_id_t id = _THIS_THREAD_ID())
    {
        _create_timer(boost::bind(func), millisec, id);
    }

    template<typename _Func, typename _Param1>
    void schedule(_Func func, _Param1 param1, int millisec, thread_id_t id = _THIS_THREAD_ID())
    {
        _create_timer(boost::bind(func, param1), millisec, id);
    }

    template<typename _Func, typename _Param1, typename _Param2>
    void schedule(_Func func, _Param1 param1, _Param2 param2, int millisec, thread_id_t id = _THIS_THREAD_ID())
    {
        _create_timer(boost::bind(func, param1, param2), millisec, id);
    }

    template<typename _Func, typename _Param1, typename _Param2, typename _Param3>
    void schedule(_Func func, _Param1 param1, _Param2 param2, _Param3 param3, int millisec, thread_id_t id = _THIS_THREAD_ID())
    {
        _create_timer(boost::bind(func, param1, param2, param3), millisec, id);
    }

    void clear()
    {
        boost::system::error_code err;
        // error is not checked, just prevent exception to be thrown.
        m_timer_impl->cancel(err);
    }

private:
    typedef unsigned int                                   timer_id_t;
    typedef boost::shared_ptr<boost::asio::deadline_timer> timer_ptr;

    template<typename _Task>
    void _create_timer(_Task taskfunc, unsigned millisec, thread_id_t id)
    {
        boost::system::error_code err;
        try
        {
            m_timer_impl = timer_ptr(new boost::asio::deadline_timer(server_app::instance().event_threadpool().service(id)));
            m_timer_impl->expires_from_now(boost::posix_time::milliseconds(millisec), err);
            if (!err)
                m_timer_impl->async_wait(taskfunc);
        }
        catch (const _threadpool_err& err)
        {
            // do nothing just don't throw up :)
        }
    }

    timer_id_t m_timerid;
    timer_ptr m_timer_impl;
};

}

#endif /* ASYNC_TASK_H_ */


