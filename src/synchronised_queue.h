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

namespace snode
{
/// Thread safe queue implementation based on STL queue.
template <typename T>
class synchronised_queue
{
public:
    synchronised_queue()
    {}

    virtual ~synchronised_queue()
    {}

    /// Insert element and notify any pending dequeue() calls.
    void enqueue(T entry)
    {
        boost::unique_lock<boost::mutex> lock_guard(mutex_lock_);
        queue_impl_.push(entry);
        condvar_.notify_one();
    }

    /// Get/remove element from queue, blocks if there are no elements into the queue.
    T dequeue()
    {
        //T result;
        boost::unique_lock<boost::mutex> lock_guard(mutex_lock_);
        while (!queue_impl_.size())
            condvar_.wait(lock_guard);

        T result = queue_impl_.front();
        queue_impl_.pop();
        return result;
    }

    /// Get element count.
    unsigned length()
    {
        return queue_impl_.size();
    }

private:
    std::queue<T> queue_impl_;
    boost::mutex  mutex_lock_;
    boost::condition_variable condvar_;
};

}
#endif /* SYNCHRONISED_QUEUE_H_ */


