//
// synchronised_queue.h
// Copyright (C) 2015  Emil Penchev, Bulgaria
//

#ifndef SYNCHRONISED_QUEUE_H_
#define SYNCHRONISED_QUEUE_H_

#include <queue>
#include <boost/thread/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/condition_variable.hpp>

namespace smkit
{
/// Thread safe queue implementation based on STL queue.
template <typename T>
class synchronised_queue
{
public:
    synchronised_queue()
    {
    }

    virtual ~synchronised_queue()
    {
    }

    /// Insert element and notify any pending dequeue() calls.
    void enqueue(T entry)
    {
        boost::unique_lock<boost::mutex> lock_guard(m_mutex_lock);
        m_queue_impl.push(entry);
        m_condvar.notify_one();
    }

    /// Get/remove element from queue, blocks if there are no elements into the queue.
    T dequeue()
    {
        T result;
        boost::unique_lock<boost::mutex> lock_guard(m_mutex_lock);
        while (!m_queue_impl.size())
            m_condvar.wait(lock_guard);

        result = m_queue_impl.front();
        m_queue_impl.pop();
        return result;
    }

    /// Get element count.
    unsigned length()
    {
        return m_queue_impl.size();
    }

private:
    std::queue<T> m_queue_impl;
    boost::mutex m_mutex_lock;
    boost::condition_variable m_condvar;
};

}
#endif /* SYNCHRONISED_QUEUE_H_ */


