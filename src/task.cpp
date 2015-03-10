#include <task.h>
#include <seq_generator.h>
#include <sstream>

namespace smkit
{

void task_scheduler::task_queue::load()
{
    boost::asio::io_service::work work(m_io_service);
    m_io_service.run();
}

void task_scheduler::task_queue::run_queue(task_queue::task_queue_impl& queue)
{
    boost::unique_lock<boost::mutex> lock(queue.m_mutex_lock);
    while (!queue.m_priority_queue.empty())
    {
        task t = queue.m_priority_queue.top();
        t.execute();
        queue.m_priority_queue.pop();
    }
}

void task_scheduler::task_queue::push(task t)
{  
    if (m_primary_queue.m_mutex_lock.try_lock())
    {
        m_primary_queue.m_priority_queue.push(t);
        m_io_service.post(boost::bind(&task_queue::run_tasks, this));
        m_primary_queue.m_mutex_lock.unlock();
    }
    else
    {
        boost::unique_lock<boost::mutex> lock(m_secondary_queue.m_mutex_lock);
        m_secondary_queue.m_priority_queue.push(t);
    }
}

void task_scheduler::task_queue::run_tasks()
{
    run_queue(m_primary_queue);
    run_queue(m_secondary_queue);
}

task_scheduler::task_scheduler()
{
}

task_scheduler::~task_scheduler()
{
}

void task_scheduler::run(unsigned threads)
{
	if (!threads)
	{
		throw task_scheduler_error("invalid thread count");
	}
	//  protect against multiple runs
	if (m_task_queues.empty())
	{
		for (unsigned idx = 0; idx < threads; idx++)
	    {
		    task_queue_ptr_t queue(new task_queue());
		    thread_ptr_t thread(new boost::thread(boost::bind(&task_queue::load, queue.get())));
		    m_task_queues.insert(std::pair<thread_id_t, task_queue_ptr_t>(thread->get_id(), queue));
			m_threads.push_back(thread);
		}
	}
}

void task_scheduler::queue_task(task t, thread_id_t tid)
{
    if (m_task_queues.count(tid) > 0)
    {
        std::map<thread_id_t, task_queue_ptr_t>::iterator it = m_task_queues.find(tid);
        if (it != m_task_queues.end())
            it->second->push(t);
        else
            throw task_scheduler_error("invalid thread id");
    }
}

int task_scheduler::queue_timer(task t, int milisec, thread_id_t tid)
{
    unsigned timer_id = 0;
    boost::system::error_code err;

    if (milisec)
    {
        if (m_task_queues.count(tid) > 0)
        {
            std::map<thread_id_t, task_queue_ptr_t>::iterator it = m_task_queues.find(tid);
            timer_ptr_t timer(new boost::asio::deadline_timer(it->second->get_io_service()));
            timer->expires_from_now(boost::posix_time::milliseconds(milisec), err);
            if (!err)
            {
                timer_id = sequence_id_generator::instance().next();
                timer->async_wait(timer_task(t, timer_id));
                boost::unique_lock<boost::mutex> lock(m_timersLock);
                m_timers.insert(std::pair<int, timer_ptr_t>(timer_id, timer));
            }
            else
            {
                throw task_scheduler_error(err.message());
            }
        }
        else
        {
            throw task_scheduler_error("invalid thread id");
        }
    }
    return timer_id;
}

void task_scheduler::clear_timer(int id)
{
    if (id)
    {
    	boost::unique_lock<boost::mutex> lock(m_timersLock);
    	std::map<int, timer_ptr_t>::iterator it = m_timers.find(id);
    	if (it != m_timers.end())
    	{
    		boost::system::error_code err;
    		m_timers.at(id)->cancel(err);
    		if (err)
    		    throw task_scheduler_error(err.message());

    		m_timers.erase(it);
    	}
    	else
    	{
    	    std::stringstream sserr;
    	    sserr << "No timer present with this ID: " << id;
    	    throw task_scheduler_error(sserr.str());
    	}
    }
}

task_scheduler& task_scheduler::instance()
{
	static task_scheduler s_scheduler;
    return s_scheduler;
}

task_scheduler::thread_id_t task_scheduler::next_thread()
{
    thread_id_t tid;
	static unsigned last_used_thread = 0;

	unsigned thread_cnt = m_threads.size();
	if (!thread_cnt)
	    throw task_scheduler_error("thread count error");

    if (last_used_thread >= thread_cnt)
        last_used_thread = 0;

	tid = m_threads[last_used_thread]->get_id();
	last_used_thread++;

	return tid;
}

boost::asio::io_service& task_scheduler::get_thread_io_service(thread_id_t tid)
{
    if (m_task_queues.count(tid) > 0)
    {
        std::map<thread_id_t, task_queue_ptr_t>::iterator it = m_task_queues.find(tid);
        return it->second->get_io_service();
    }
    else
    {
        std::stringstream sserr;
        sserr << "Invalid thread id: " << tid;
        throw task_scheduler_error(sserr.str());
    }
}

timer_task::timer_task(const task& t, int id)
 : task(t), m_timer_id(id)
{
}

void timer_task::operator()(const boost::system::error_code& error)
{
	if (!error) m_functor();
    task_scheduler::instance().clear_timer(m_timer_id);
}

task::task(boost::function<void()> func, task_priority prio)
 : m_functor(func), m_priority(prio)
{
}

task::task(const task& t)
{
	this->m_functor = t.m_functor;
	this->m_priority = t.m_priority;
}

void task::execute()
{
	m_functor();
}

} // end of namespace smkit

