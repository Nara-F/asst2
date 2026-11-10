#ifndef _TASKSYS_H
#define _TASKSYS_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include "itasksys.h"

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial : public ITaskSystem {
   public:
    TaskSystemSerial(int num_threads);
    ~TaskSystemSerial();
    const char* name();
    void run(IRunnable* runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                            const std::vector<TaskID>& deps);
    void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn : public ITaskSystem {
   public:
    TaskSystemParallelSpawn(int num_threads);
    ~TaskSystemParallelSpawn();
    const char* name();
    void run(IRunnable* runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                            const std::vector<TaskID>& deps);
    void sync();
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning : public ITaskSystem {
   public:
    TaskSystemParallelThreadPoolSpinning(int num_threads);
    ~TaskSystemParallelThreadPoolSpinning();
    const char* name();
    void run(IRunnable* runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                            const std::vector<TaskID>& deps);
    void sync();
};

/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping : public ITaskSystem {
   public:
    struct Task {
        IRunnable* runnable;
        int task_idx;
        int num_total_tasks;
        int task_id;
    };
    struct PendingTask {
        IRunnable* runnable;
        int num_total_tasks;
        std::vector<TaskID> deps;
        TaskID task_id;
    };

    int num_threads;
    TaskID curTask;
    std::mutex task_mutex;
    std::vector<int> task_remaining;
    std::deque<Task> task_queue;
    std::mutex queue_mutex;
    std::vector<std::thread> threads;

    std::mutex set_mutex;
    std::unordered_set<TaskID> finished_tasks;

    std::mutex pending_set_mutex;
    std::vector<PendingTask> pending_tasks;

    // Zero-task batches count as one until their dependencies are satisfied.
    int pending_task_count = 0;
    std::mutex finished_task_mutex;
    std::condition_variable finished_task_cv;

    std::vector<int>
        status_call;  //-1 for destruction,0 for waiting, 1 for running
    std::vector<std::mutex> status_mutex;

    std::vector<std::condition_variable> cv;

    TaskSystemParallelThreadPoolSleeping(int num_threads);
    ~TaskSystemParallelThreadPoolSleeping();
    const char* name();
    bool insertingTask(PendingTask& task);
    void run(IRunnable* runnable, int num_total_tasks);
    TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                            const std::vector<TaskID>& deps);
    void sync();
};

#endif
