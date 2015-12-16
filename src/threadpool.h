//
// threadpool.h
// Copyright (C) 2015  Emil Penchev, Bulgaria
//

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <map>
#include <vector>
#include <functional>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "synchronised_queue.h"
#include "thread_wrapper.h"
#include "snode_types.h"
#include "async_op.h"

namespace snode
{

/// errors
struct cancel_thread_err {};
static const char* s_threadpool_msg = "invalid thread id";

/// General purpose thread pool,
/// creates a dedicated pool of threads for task/operations that will slow down or interrupt I/O handling.
class threadpool
{
public:

    threadpool(size_t size = 1) : size_(size)
    {
        // pool is created and started
        for (size_t i = 0; i < size_; i++)
        {
            thread_ptr thread = std::make_shared<snode::lib::thread>(std::bind(&threadpool::start_thread, this));
            threads_.push_back(thread);
        }
    }

    ~threadpool()
    {
        stop();
    }

    void stop()
    {
        // prevent a second stop
        if (threads_.empty() || queues_index_.empty())
            return;

        for (auto iter = threads_.begin(); iter != threads_.end(); ++iter)
        {
            thread_ptr thread = *iter;
            stop_thread(thread->get_id());
            thread->join();
        }

        // clear all threads and queues run() will create new ones
        threads_.clear();
        queues_index_.clear();
    }

    /// Post a task to a given thread. Task can be anything as long it has () operator defined.
    /// Throws runtime_error on error.
    //template<typename T>
    void schedule(async_op_base op, thread_id_t id)
    {
        // match the task queue with thread id
        auto it = queues_index_.find(id);

        if (queues_index_.end() == it)
            throw std::runtime_error(s_threadpool_msg);

        it->second->enqueue(op);
    }

    /// Get the direct thread interface pool
    const std::vector<thread_ptr>& threads() const
    {
        return threads_;
    }

private:

    //typedef synchronised_queue< std::function<void()> > task_queue_t;
    typedef synchronised_queue<async_op_base> task_queue_t;
    typedef std::shared_ptr<task_queue_t> task_queue_ptr;

    /// Thread entry function.
    void start_thread()
    {
        task_queue_ptr queue(new task_queue_t);
        queues_index_.insert(std::pair<thread_id_t, task_queue_ptr>(boost::this_thread::get_id(), queue));
        while (true)
        {
            try
            {
                auto task = queue->dequeue();
                task.run();
                //task();
            }
            catch (const cancel_thread_err&)
            {
                // thread was cancelled,
                // all queued tasks will not be executed
                return;
            }
            catch (...)
            {
                // Something bad happened
                throw;
            }
        }
    }

    /// Stop thread, internal method.
    void stop_thread(thread_id_t id)
    {
        //schedule([]() -> void { throw cancel_thread_err(); }, id);
    }

    std::size_t size_;
    std::vector<thread_ptr> threads_;
    std::map<thread_id_t, task_queue_ptr> queues_index_;
};

}
#endif /* THREADPOOL_H_ */

