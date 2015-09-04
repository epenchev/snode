//
// threadpool.h
// Copyright (C) 2015  Emil Penchev, Bulgaria
//

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <map>
#include <vector>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/locks.hpp>
#include "synchronised_queue.h"

namespace snode
{

typedef boost::thread::id thread_id_t;
typedef boost::shared_ptr<boost::thread> thread_ptr;
typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

#define THIS_THREAD_ID() boost::this_thread::get_id()

/// errors
struct cancel_thread_err {};
struct threadpool_err {};

/// I/O event thread pool based on boost io_service class.
/// For every thread a new instance of io_service is created,
/// this is guarantee that each thread is running a separate I/O event loop.
class io_event_threadpool
{
public:
    io_event_threadpool(size_t pool_size = 1) : next_io_service_(0)
    {
        // Give all the io_services work to do so that their run() functions will not
        // exit until they are explicitly stopped.
        for (unsigned i = 0; i < pool_size; ++i)
        {
            io_service_ptr io_service(new boost::asio::io_service);
            work_ptr work(new boost::asio::io_service::work(*io_service));
            io_services_.push_back(io_service);
            work_.push_back(work);
        }

        // Run a io_service over the main server thread
        threads_index_.insert(std::pair<thread_id_t, size_t>(boost::this_thread::get_id(), 0));
    }

    ~io_event_threadpool()
    {
        for (auto iter = threads_.begin(); iter != threads_.end(); ++iter)
        {
            thread_ptr thread = *iter;
            stop_thread(thread->get_id());
            thread->join();
        }
    }

    /// Start all I/O service event loops/threads
    void run()
    {
        for (unsigned idx = 1; idx < io_services_.size(); idx++)
        {
            thread_ptr thread(new boost::thread(boost::bind(&io_event_threadpool::start_thread, this, idx)));
            threads_.push_back(thread);
            threads_index_.insert(std::pair<thread_id_t, size_t>(thread->get_id(), idx));
        }
        io_services_[0]->run();
    }

    /// Stop all I/O service event loops/threads
    void stop()
    {
        auto it = threads_index_.find(boost::this_thread::get_id());
        if (it != threads_index_.end())
        {
            // main thread is calling this proceed with stop
            if (0 == it->second)
            {
                for (auto iter = threads_.begin(); iter != threads_.end(); ++iter)
                {
                    thread_ptr thread = *iter;
                    stop_thread(thread->get_id());
                    thread->join();
                }
                // clear all threads run() will create new ones
                threads_.clear();

                io_services_[0]->stop();
            }
        }
    }

    /// Post a task to a given event loop/thread. Task can be anything as long it has () operator defined.
    template<typename T>
    void schedule(T task, thread_id_t id = THIS_THREAD_ID())
    {
        auto it = threads_index_.find(id);
        if (it != threads_index_.end())
        {
            io_services_[it->second]->post(task);
        }
    }

    boost::asio::io_service& service(thread_id_t id)
    {
        auto it = threads_index_.find(id);
        if (it != threads_index_.end())
        {
            return *io_services_[it->second];
        }
        else
        {
            throw threadpool_err();
        }
    }

    /// Return a io_service instance for a I/O object to be attached (ex. socket).
    boost::asio::io_service& io_service()
    {
        // Use a round-robin scheme to choose the next io_service to use.
        boost::asio::io_service& io_service = *io_services_[next_io_service_++];
        if (next_io_service_ == io_services_.size())
        {
            next_io_service_ = 0;
        }
        return io_service;
    }

    /// Mostly used by server_app class to make server_handler => thread association.
    const std::vector<thread_ptr>& threads()
    {
        return threads_;
    }

private:
    /// Thread entry function.
    void start_thread(std::size_t idx)
    {
        try
        {
            io_services_[idx]->run();
        }
        catch (const cancel_thread_err&)
        {
            // thread was cancelled, all queued event tasks will get error code
            // operation canceled
            if (!io_services_[idx]->stopped())
                io_services_[idx]->stop();
        }
        catch (...)
        {
            // Something bad happened
            throw;
        }
    }

    /// Kill the thread/event loop
    void stop_thread(thread_id_t id)
    {
        // no check for the id, internal method
        schedule([]() -> void { throw cancel_thread_err(); }, id);
    }

    size_t next_io_service_;
    std::vector<work_ptr> work_;
    std::vector<io_service_ptr> io_services_;
    std::vector<thread_ptr> threads_;
    std::map<thread_id_t, size_t> threads_index_; // thread -> io_service index map
};

/// General purpose thread pool,
/// creates a dedicated pool of threads for task/operations that will slow down or interrupt I/O handling.
/// Example of such a task may be media transcoding, which is heavy CPU consuming task.
class sys_processor_threadpool
{
public:

    typedef synchronised_queue< boost::function<void()> > task_queue_t;
    typedef boost::shared_ptr<task_queue_t> task_queue_ptr;

    sys_processor_threadpool(size_t pool_size = 1) : pool_size_(pool_size)
    {}

    ~sys_processor_threadpool()
    {
        stop();
    }

    void run()
    {
        for (size_t i = 0; i < pool_size_; i++)
        {
            thread_ptr thread(new boost::thread(boost::bind(&sys_processor_threadpool::start_thread, this)));
            threads_.push_back(thread);
        }
    }

    void stop()
    {
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

    /// Post a task to a given event loop/thread. Task can be anything as long it has () operator defined.
    /// Throws threadpool_err on error.
    template<typename T>
    void schedule(T task, thread_id_t id)
    {
        auto it = queues_index_.find(id);
        if (it == queues_index_.end())
        {
            throw threadpool_err();
        }
        else if (id == it->first)
        {
            it->second->enqueue(task);
        }
    }

private:

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
                task();
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
        schedule([]() -> void { throw cancel_thread_err(); }, id);
    }

    std::size_t pool_size_;
    std::vector<thread_ptr> threads_;
    std::map<thread_id_t, task_queue_ptr> queues_index_;
};

}
#endif /* THREADPOOL_H_ */

