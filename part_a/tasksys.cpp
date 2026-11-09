#include "tasksys.h"

#include <queue>
#include <thread>

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}
/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() { return "Serial"; }

TaskSystemSerial::TaskSystemSerial(int num_threads)
    : ITaskSystem(num_threads) {}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable,
                                          int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
    : ITaskSystem(num_threads), num_threads(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

#include <iostream>
using std::cerr;
void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    int maxThread = std::min(num_total_tasks, num_threads);
    std::vector<std::thread> threads(maxThread);

    auto thread_Run = [&](IRunnable* runnable, int num_total_tasks) {
        TaskID task_id;
        do {
            {
                std::lock_guard<std::mutex> lock(task_id_mutex);
                task_id = curTask;
                curTask++;
            }
            if (task_id >= num_total_tasks) break;
            runnable->runTask(task_id, num_total_tasks);
        } while (true);
    };

    TaskSystemParallelSpawn::curTask = 0;
    for (int i = 0; i < maxThread; i++) {
        threads[i] = std::thread(thread_Run, runnable, num_total_tasks);
    }
    for (int i = 0; i < maxThread; i++) {
        threads[i].join();
    }

    // One single distribution implementation
    // for (int i = 0; i < num_total_tasks; i++) {
    //     if (i >= maxThread) threads[i % maxThread].join();
    //     threads[i % maxThread] = std::thread([runnable, i, num_total_tasks]()
    //     {
    //         runnable->runTask(i, num_total_tasks);
    //     });
    // }
    // for (int i = 0; i < maxThread; i++) {
    //     threads[i].join();
    // }

    // WorkQueue implementation
    // std::vector<bool> free(maxThread, true);
    // std::queue<int> free_Threads;
    // for (int i = 0; i < maxThread; i++) {
    //     free_Threads.push(i);
    // }
    // int task_left = num_total_tasks;
    // while (task_left > 0) {
    //     for (int i = 0; i < maxThread; i++) {
    //         if (!free[i]) {
    //             if (threads[i].joinable()) {
    //                 threads[i].join();
    //                 free[i] = true;
    //                 free_Threads.push(i);
    //                 // active_threads--;
    //             }
    //         }
    //     }
    //     while (free_Threads.size() && task_left) {
    //         auto i = free_Threads.front();
    //         free_Threads.pop();
    //         threads[i] = std::thread([runnable, task_left, num_total_tasks]()
    //         {
    //             runnable->runTask(task_left - 1, num_total_tasks);
    //         });
    //         task_left--;
    //         free[i] = false;
    //         // active_threads++;
    //     }
    // }
    // for (int i = 0; i < maxThread; i++) {
    //     if (!free[i]) threads[i].join();
    // }

    // Serial implementation for testing
    // for (int i = 0; i < num_total_tasks; i++) {
    //     runnable->runTask(i, num_total_tasks);
    // }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(
    int num_threads)
    : ITaskSystem(num_threads),
      num_threads(num_threads),
      work_queues_mutexs(num_threads) {
    threads.resize(num_threads);
    work_queues.resize(num_threads);

    auto waiting = [&](int idx) {
        Task task;
        while (true) {
            {
                std::lock_guard<std::mutex> lock(work_queues_mutexs[idx]);
                task = work_queues[idx];
            }
            if (task.runnable == nullptr && task.num_total_tasks == -1) break;
            if (task.runnable == nullptr && task.num_total_tasks == 0) continue;

            TaskID task_id;
            do {
                {
                    std::lock_guard<std::mutex> lock(task_id_mutex);
                    task_id = curTask;
                    curTask++;
                }
                if (task_id >= task.num_total_tasks) break;
                task.runnable->runTask(task_id, task.num_total_tasks);
            } while (true);

            {
                std::lock_guard<std::mutex> lock(work_queues_mutexs[idx]);
                work_queues[idx].runnable = nullptr;
                work_queues[idx].num_total_tasks = 0;
            }
            {
                std::lock_guard<std::mutex> lock(finish_mutex);
                finish_count++;
            }
        }
    };

    for (int i = 0; i < num_threads; i++) {
        threads[i] = std::thread(waiting, i);
    }
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    for (int i = 0; i < num_threads; i++) {
        {
            std::lock_guard<std::mutex> lock(work_queues_mutexs[i]);
            work_queues[i] = Task{nullptr, -1};
        }
    }
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable,
                                               int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    int maxThread = std::min(num_total_tasks, num_threads);
    {
        std::lock_guard<std::mutex> lock(task_id_mutex);
        curTask = 0;
    }
    {
        std::lock_guard<std::mutex> lock(finish_mutex);
        finish_count = 0;
    }
    {
        for (int i = 0; i < maxThread; i++) {
            {
                std::lock_guard<std::mutex> lock(work_queues_mutexs[i]);
                work_queues[i] = Task{runnable, num_total_tasks};
            }
        }
    }
    while (true) {
        TaskID task_id;
        {
            std::lock_guard<std::mutex> lock(finish_mutex);
            if (finish_count >= maxThread) {
                break;
            }
        }
        // for (int i = 0; i < num_total_tasks; i++) {
        //     runnable->runTask(i, num_total_tasks);
        // }
    }
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
    return;
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(
    int num_threads)
    : ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable,
                                               int num_total_tasks) {
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
    //
    // TODO: CS149 students will modify the implementation of this method in
    // Part B.
    //

    return;
}
