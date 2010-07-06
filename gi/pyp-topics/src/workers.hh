#ifndef WORKERS_HH
#define WORKERS_HH

#include <iostream>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/future.hpp>

//#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "timing.h"

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
