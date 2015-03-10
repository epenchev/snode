//
// task.h
// Copyright (C) 2014  Emil Penchev, Bulgaria

#ifndef TASK_H_
#define TASK_H_

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>
#include <queue>
#include <map>
#include <vector>

namespace smkit
{
enum task_priority { Normal = 0, High };

/// Class that represents a functor object handler created with boost::bind.
/// Task objects are asynchronous operations executed within a specific thread.
class task
{
public:
	task(const task& t);

	/// Create task as boost functor object, and assign a priority for the task_scheduler
	/// to take in mind when running the task.

	template <class Func>
	static inline task connect(Func f)
	{
		return task( boost::bind(f), Normal );
	}

    template <class Func, class T1>
    static inline task connect(Func f, T1 p1)
    {
        return task( boost::bind(f, p1), Normal );
    }

    template <class Func, class T1, class T2>
    static inline task connect(Func f, T1 p1, T2 p2)
    {
        return task( boost::bind(f, p1, p2), Normal );
    }

    template <class Func, class T1, class T2, class T3>
    static inline task connect(Func f, T1 p1, T2 p2, T3 p3)
    {
        return task( boost::bind(f, p1, p2, p3), Normal );
    }

    template <class Func, class T1, class T2, class T3, class T4>
    static inline task connect(Func f, T1 p1, T2 p2, T3 p3, T4 p4)
    {
        return task( boost::bind(f, p1, p2, p3, p4), Normal );
    }
    
    template <class Func, class T1, class T2, class T3, class T4, class T5>
    static inline task connect(Func f, T1 p1, T2 p2, T3 p3, T4 p4, T5 p5)
    {
        return task( boost::bind(f, p1, p2, p3, p4, p5), Normal );
    }

    /// Run the task.
    void execute();

    /// std::priority_queue compare function, compares two tasks based on their priority.
    friend bool operator < (const task& a, const task& b) { return a.m_priority < b.m_priority; }
    
protected:
    boost::function<void()>   m_functor;     // boost functor object
    task_priority             m_priority;    // task priority

    /// Disable direct instance creation, tasks are only created via the connect() method
    task(boost::function<void()> func, task_priority prio);
};

/// An extended version of the task class which represents a boost timer functor handler.
class timer_task : public task
{
public:
	timer_task(const task& task, int id);
	void operator()( const boost::system::error_code& error ); // timer handler
private:
	int m_timer_id;
};

class task_scheduler : boost::noncopyable
{
	task_scheduler();

public:
    ~task_scheduler();

    typedef boost::thread::id                              thread_id_t;
    typedef boost::shared_ptr<boost::thread>               thread_ptr_t;
    typedef boost::shared_ptr<boost::asio::deadline_timer> timer_ptr_t;

    /// public API
    /// Starts count threads each running a separate io_service loop.
    void run(unsigned threads);

    /// Insert a task into a task queue object associated within the thread id (tid).
    /// Task will be later executed in the thread context.
    void queue_task(task t, thread_id_t tid = boost::this_thread::get_id());

    /// Create a timer and add the task (t) for later execution. Timer will execute task after milisec timeout
    /// within the thread id (tid).
    /// Returns the timer id which can be used later for timer cancellation.
    int queue_timer(task t, int milisec, thread_id_t tid = boost::this_thread::get_id());

    /// Cancel/stop the timer by a given timer id.
    void clear_timer(int timerid);

    /// Get the task_sheduler instance.
    /// An instance of task_sheduler can't be created it is only one instance created.
    static task_scheduler& instance();

    /// Get next available thread (round robin scheduling) from the pool of preallocated threads.
    thread_id_t next_thread();

    /// Get boost io_service object associated with boost thread id.
    /// IO service is used to create asynchronous IO objects like sockets.
    boost::asio::io_service& get_thread_io_service(thread_id_t tid);

private:
    
    /// Inner class abstracting the usage of the boost io_service class.
    class task_queue
    {
    public:
        
        struct task_queue_impl
        {
            std::priority_queue<task> m_priority_queue;
            boost::mutex m_mutex_lock;
        }; 

        /// io_service post handler
        void run_tasks();

        /// push a task into the queue for later execution
        void push(task task);

        /// Starts the boost io_service loop
        void load();

        /// Execute all tasks in the queue until the queue is empty.
        /// This call is executed each time a task is pushed into a task queue.
        void run_queue(task_queue_impl& qimpl);

        boost::asio::io_service& get_io_service() { return m_io_service; }

    private:
        task_queue_impl m_primary_queue;
        task_queue_impl m_secondary_queue;
        boost::asio::io_service m_io_service;
    };
    
    typedef boost::shared_ptr<task_queue> task_queue_ptr_t;

    std::vector<thread_ptr_t>                m_threads;
    std::map<thread_id_t, task_queue_ptr_t>  m_task_queues;
    boost::mutex                             m_timersLock;
    std::map<int, timer_ptr_t>               m_timers;

    task_queue_ptr_t lookup_queue(boost::thread::id tid);
};

class task_scheduler_error : public std::exception
{
public:
    task_scheduler_error(const std::string& message)
    {
        m_msg = message;
    }

    ~task_scheduler_error() throw()
    {
    }

    virtual const char* what() throw()
    {
        return m_msg.c_str();
    }
protected:
    std::string m_msg;
};
} // end of namespace smkit
#endif // TASK_H_


