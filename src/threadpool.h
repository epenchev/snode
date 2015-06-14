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

namespace smkit
{

typedef boost::thread::id thread_id_t;
typedef boost::shared_ptr<boost::thread> thread_ptr;
typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

#define _THIS_THREAD_ID() boost::this_thread::get_id()

/// errors
struct _cancel_thread {};
struct _threadpool_err {};

/// I/O event thread pool based on boost io_service class.
/// For every thread a new instance of io_service is created,
/// this is guarantee that each thread is running a separate I/O event loop.
class io_event_threadpool
{
public:
    io_event_threadpool(size_t pool_size = 1) : m_next_io_service(0)
    {
        // Give all the io_services work to do so that their run() functions will not
        // exit until they are explicitly stopped.
        for (unsigned i = 0; i < pool_size; ++i)
        {
            io_service_ptr io_service(new boost::asio::io_service);
            work_ptr work(new boost::asio::io_service::work(*io_service));
            m_io_services.push_back(io_service);
            m_work.push_back(work);
        }
    }

    ~io_event_threadpool()
    {
        for (auto iter = m_threads.begin(); iter != m_threads.end(); ++iter)
        {
            thread_ptr thread = *iter;
            _stop_thread(thread->get_id());
            thread->join();
        }
    }

    /// Start all I/O service event loops/threads
    void run()
    {
        for (unsigned idx = 0; idx < m_io_services.size(); idx++)
        {
            thread_ptr thread(new boost::thread(boost::bind(&io_event_threadpool::_start_thread, this, idx)));
            m_threads.push_back(thread);
            m_threads_index.insert(std::pair<thread_id_t, size_t>(thread->get_id(), idx));
        }
    }

    /// Post a task to a given event loop/thread. Task can be anything as long it has () operator defined.
    template<typename T>
    void schedule(T task, thread_id_t id = _THIS_THREAD_ID())
    {
        auto it = m_threads_index.find(id);
        if (it != m_threads_index.end())
            m_io_services[it->second]->post(task);
    }

    boost::asio::io_service& service(thread_id_t id)
    {
        auto it = m_threads_index.find(id);
        if (it != m_threads_index.end())
        {
            return *m_io_services[it->second];
        }
        else
        {
            throw _threadpool_err();
        }
    }

    /// Return a io_service instance for a I/O object to be attached (ex. socket).
    boost::asio::io_service& service()
    {
        // Use a round-robin scheme to choose the next io_service to use.
        boost::asio::io_service& io_service = *m_io_services[m_next_io_service];
        ++m_next_io_service;
        if (m_next_io_service == m_io_services.size())
        {
            m_next_io_service = 0;
        }
        return io_service;
    }

    /// Mostly used by server_app class to make server_handler => thread association.
    const std::vector<thread_ptr>& threads()
    {
        return m_threads;
    }

private:
    /// Thread entry function.
    void _start_thread(std::size_t idx)
    {
        try
        {
            m_io_services[idx]->run();
        }
        catch (const _cancel_thread&)
        {
            // thread was cancelled
        }
        catch (...)
        {
            // Something bad happened
            throw;
        }
    }

    /// Kill the thread/event loop
    void _stop_thread(thread_id_t id)
    {
        // no check for the id, internal method
        schedule([]() -> void { throw _cancel_thread(); }, id);
    }

    size_t m_next_io_service;
    std::vector<work_ptr> m_work;
    std::vector<io_service_ptr> m_io_services;
    std::vector<thread_ptr> m_threads;
    std::map<thread_id_t, size_t> m_threads_index;
};

/// General purpose thread pool,
/// creates a dedicated pool of threads for task/operations that will slow down or interrupt I/O handling.
/// Example of such a task may be media transcoding, which is heavy CPU consuming task.
class sys_processor_threadpool
{
public:

    typedef synchronised_queue< boost::function<void()> > task_queue_t;
    typedef boost::shared_ptr<task_queue_t> task_queue_ptr;

    sys_processor_threadpool()
    {
    }

    ~sys_processor_threadpool()
    {
    }

    /// Add/create a new thread to the pool.
    thread_id_t add_thread()
    {
        thread_ptr thread(new boost::thread(boost::bind(&sys_processor_threadpool::_start_thread, this)));
        m_threads_index.insert(std::pair<thread_id_t, thread_ptr>(thread->get_id(), thread));
        return thread->get_id();
    }

    /// Post a task to a given event loop/thread. Task can be anything as long it has () operator defined.
    /// Throws _threadpool_err on error.
    template<typename T>
    void schedule(T task, thread_id_t id)
    {
        boost::unique_lock<boost::mutex> autolock(m_queues_lock);
        auto it = m_queues_index.find(id);
        if (it == m_queues_index.end())
        {
            throw _threadpool_err();
        }
        else if (id == it->first)
        {
            it->second->enqueue(task);
        }
    }
    
    /// Stop/delete thread.
    void drop_thread(thread_id_t id)
    {
        boost::unique_lock<boost::mutex> autolock(m_queues_lock);
        auto it = m_threads_index.find(id);
        if (it != m_threads_index.end())
        {
            schedule([]() -> void { throw _cancel_thread(); }, id);
        }
    }

private:

    /// Thread entry function.
    void _start_thread()
    {
        task_queue_ptr queue(new task_queue_t);
        m_queues_index.insert(std::pair<thread_id_t, task_queue_ptr>(boost::this_thread::get_id(), queue));
        while (true)
        {
            try
            {
                auto task = queue->dequeue();
                task();
            }
            catch (const _cancel_thread&)
            {
                // thread was cancelled
                m_threads_index.erase(m_threads_index.find(boost::this_thread::get_id()));
                m_queues_index.erase(m_queues_index.find(boost::this_thread::get_id()));
                return;
            }
            catch (...)
            {
                // Something bad happened
                throw;
            }
        }
    }

    boost::mutex m_queues_lock;
    std::map<thread_id_t, thread_ptr> m_threads_index;
    std::map<thread_id_t, task_queue_ptr> m_queues_index;
};

}
#endif /* THREADPOOL_H_ */

