//
// async_timer.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef _ASYNC_TIMER_H_
#define _ASYNC_TIMER_H_

#include <memory>
#include <functional>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "seq_generator.h"
#include "threadpool.h"

namespace snode
{

class async_timer
{
public:
    async_timer()
      : timerid_(sequence_id_generator::instance().next()), timer_(nullptr)
    {}

    ~async_timer()
    {
        clear();
    }

    template<typename F>
    void schedule(F func, unsigned millisec, thread_id_t id = THIS_THREAD_ID())
    {
        create_timer(func, millisec, id);
    }

    template<typename F, typename A1>
    void schedule(F func, A1 a1, int millisec, thread_id_t id = THIS_THREAD_ID())
    {
        create_timer(std::bind(func, a1), millisec, id);
    }

    template<typename F, typename A1, typename A2>
    void schedule(F func, A1 a1, A2 a2, int millisec, thread_id_t id = THIS_THREAD_ID())
    {
        create_timer(boost::bind(func, a1, a2), millisec, id);
    }

    template<typename Func, typename A1, typename A2, typename A3>
    void schedule(Func func, A1 a1, A2 a2, A3 a3, int millisec, thread_id_t id = THIS_THREAD_ID())
    {
        create_timer(boost::bind(func, a1, a2, a3), millisec, id);
    }

    template<typename Func, typename A1, typename A2, typename A3, typename A4>
    void schedule(Func func, A1 a1, A2 a2, A3 a3, A4 a4, int millisec, thread_id_t id = THIS_THREAD_ID())
    {
        create_timer(boost::bind(func, a1, a2, a3, a4), millisec, id);
    }

    template<typename Func, typename A1, typename A2, typename A3, typename A4, typename A5>
    void schedule(Func func, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, int millisec, thread_id_t id = THIS_THREAD_ID())
    {
        create_timer(boost::bind(func, a1, a2, a3, a4, a5), millisec, id);
    }

    void clear()
    {
        boost::system::error_code err;
        timer_->cancel(err);
    }

private:
    typedef unsigned int                                   timer_id_t;
    typedef std::shared_ptr<boost::asio::deadline_timer>   timer_ptr;

    template<typename TimerHandler>
    void create_timer(TimerHandler handler, unsigned millisec, thread_id_t id)
    {
        boost::system::error_code err;
        try
        {
            timer_ = std::make_shared<boost::asio::deadline_timer>(snode_core::instance().get_io_service());
            timer_->expires_from_now(boost::posix_time::milliseconds(millisec), err);
            if (!err)
            {
                timer_->async_wait(handler);
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

#endif /* ASYNC_TIMER_H_ */
