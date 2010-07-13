/**
  Basic thread-pool tools using Boost.Thread.
  (Jan Botha, 7/2010)

  --Simple usage--
  Use SimpleWorker.
    Example, call a function that returns an int in a new thread:
    typedef boost::function<int()> JobType;
    JobType job = boost::bind(funcname);
      //or boost::bind(&class::funcname, this) for a member function
    SimpleWorker<JobType, int> worker(job);
    int result = worker.getResult(); //blocks until result is ready

  --Extended usage--
  Use WorkerPool, which uses Queuemt (a synchronized queue) and Worker.
  Example:
    (same context and typedef
    WorkerPool<JobType, int> pool(num_threads);
    JobType job = ...
    pool.addJob(job);
    ...
    pool.get_result(); //blocks until all workers are done, returns the some of their results.  
    
    Jobs added to a WorkerPool need to be the same type. A WorkerPool instance should not be reused (e.g. adding jobs) after calling get_result(). 
*/

#ifndef WORKERS_HH
#define WORKERS_HH

#include <iostream>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <queue>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/condition.hpp>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include "timing.h"

/** Implements a synchronized queue*/
template<typename J>
class Queuemt
{

public:
    boost::condition_variable_any cond;
    const bool& running;

    Queuemt() { }
    Queuemt(const bool& running) : running(running), maxsize(0), qsize(0) 
    { 
    }

    ~Queuemt() { 
     }

    J pop()
    {
        J job;
        {
            boost::unique_lock<boost::shared_mutex> qlock(q_mutex);
            while (running && qsize == 0)
                cond.wait(qlock);

            if (qsize > 0)
            {
                job = q.front();
                q.pop();
                --qsize;      
            }
        }
        if (job)
            cond.notify_one();
        return job;

    }

    void push(J job)
    {
        {
            boost::unique_lock<boost::shared_mutex> lock(q_mutex);
            q.push(job);
            ++qsize;
        }
        if (qsize > maxsize)
            maxsize = qsize;
        
        cond.notify_one();
    }

    int getMaxsize()
    {
        return maxsize;
    }

    int size()
    {
        return qsize;
    }

private:
    boost::shared_mutex q_mutex;
    std::queue<J> q;
    int maxsize;
    volatile int qsize;
};


template<typename J, typename R>
class Worker
{
typedef boost::packaged_task<R> PackagedTask;
public:
    Worker(Queuemt<J>& queue, int id, int num_workers) :  
      q(queue), tasktime(0.0), id(id), num_workers(num_workers)
    {
        PackagedTask task(boost::bind(&Worker<J, R>::run, this));
        future = task.get_future();
        boost::thread t(boost::move(task));
    }

    R run() //this is called upon thread creation
    {
        R wresult = 0;
        while (isRunning())
        {
            J job = q.pop();

            if (job)
            {
                timer.Reset();
                wresult += job();
                tasktime += timer.Elapsed();
            }
        }
        return wresult;
    }

    R getResult()
    {
        if (!future.is_ready())
            future.wait();
        assert(future.is_ready());
        return future.get();
    }

    double getTaskTime()
    {
        return tasktime;
    }

private:

    Queuemt<J>& q;

    boost::unique_future<R> future;

    bool isRunning()
    {
        return q.running || q.size() > 0;
    }
    
    Timer timer;
    double tasktime;
    int id;
    int num_workers;
};

template<typename J, typename R>
class WorkerPool
{
typedef boost::packaged_task<R> PackagedTask;
typedef Worker<J,R> WJR;
typedef boost::ptr_vector<WJR> WorkerVector;
public:

    WorkerPool(int num_workers)
    {
        q.reset(new Queuemt<J>(running));
        running = true;
        for (int i = 0; i < num_workers; ++i)
            workers.push_back( new Worker<J, R>(*q, i, num_workers) );
    }

    ~WorkerPool()
    {
    }

    R get_result()
    {
        running = false;
        q->cond.notify_all();
        R tmp = 0;
        double tasktime = 0.0;
        for (typename WorkerVector::iterator it = workers.begin(); it != workers.end(); it++)
        {
            R res = it->getResult();
            tmp += res;
            //std::cerr << "tasktime: " << it->getTaskTime() << std::endl; 
            tasktime += it->getTaskTime();
        }
//        std::cerr << " maxQ = " << q->getMaxsize() << std::endl;
        return tmp;
    }

    void addJob(J job)
    {
        q->push(job);
    }

private:

    WorkerVector workers;

    boost::shared_ptr<Queuemt<J> > q;

    bool running;
};

///////////////////
template <typename J, typename R>
class SimpleWorker
{
typedef boost::packaged_task<R> PackagedTask;
public:
    SimpleWorker(J& job) : job(job), tasktime(0.0)
    {
        PackagedTask task(boost::bind(&SimpleWorker<J, R>::run, this));
        future = task.get_future();
        boost::thread t(boost::move(task));
    }

    R run() //this is called upon thread creation
    {
        R wresult = 0;

        assert(job);
        timer.Reset();
        wresult = job();
        tasktime = timer.Elapsed();
        std::cerr << tasktime << " s" << std::endl; 
        return wresult;
    }

    R getResult()
    {
        if (!future.is_ready())
            future.wait();
        assert(future.is_ready());
        return future.get();
    }

    double getTaskTime()
    {
        return tasktime;
    }

private:

    J job;

    boost::unique_future<R> future;

    Timer timer;
    double tasktime;

};



#endif 
