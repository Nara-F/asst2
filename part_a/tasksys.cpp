#include "tasksys.h"

#include <algorithm>
#include <condition_variable>
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
    return 0;
}

void TaskSystemSerial::sync() { return; }

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
    : ITaskSystem(num_threads), num_threads(num_threads) {}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

#include <iostream>
using std::cerr;
void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
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
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    return 0;
}

void TaskSystemParallelSpawn::sync() { return; }

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
      work_queues_mutexs(std::max(1, num_threads - 1)) {
    this->num_threads = std::max(1, num_threads - 1);
    threads.resize(this->num_threads);
    work_queues.resize(this->num_threads);

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

    for (int i = 0; i < this->num_threads; i++) {
        threads[i] = std::thread(waiting, i);
    }
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
        {
            std::lock_guard<std::mutex> lock(finish_mutex);
            if (finish_count >= maxThread) {
                break;
            }
        }
    }
}

void TaskSystemParallelThreadPoolSpinning::sync() { return; }

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
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
    : ITaskSystem(num_threads),
      work_queues_mutexs(std::max(1, num_threads - 1)),
      cv(std::max(1, num_threads - 1)),
      num_threads(std::max(1, num_threads - 1)) {
    threads.resize(this->num_threads);
    work_queues.resize(this->num_threads);

    auto waiting = [&](int idx) {
        Task task;
        while (true) {
            {
                std::unique_lock<std::mutex> lock(work_queues_mutexs[idx]);
                cv[idx].wait(lock, [=] {
                    return work_queues[idx].runnable != nullptr ||
                           work_queues[idx].num_total_tasks == -1;
                });
                lock.unlock();
                task = work_queues[idx];
            }
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

    for (int i = 0; i < this->num_threads; i++) {
        threads[i] = std::thread(waiting, i);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    for (int i = 0; i < num_threads; i++) {
        {
            std::lock_guard<std::mutex> lock(work_queues_mutexs[i]);
            work_queues[i] = Task{nullptr, -1};
            cv[i].notify_one();
        }
    }
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable,
                                               int num_total_tasks) {
    int maxThread = std::min(num_total_tasks, num_threads);
    {
        std::lock_guard<std::mutex> lock(task_id_mutex);
        curTask = 0;
    }
    {
        std::lock_guard<std::mutex> lock(finish_mutex);
        finish_count = 0;
    }
    for (int i = 0; i < maxThread; i++) {
        {
            std::lock_guard<std::mutex> lock(work_queues_mutexs[i]);
            work_queues[i] = Task{runnable, num_total_tasks};
            cv[i].notify_one();
        }
    }
    while (true) {
        {
            std::lock_guard<std::mutex> lock(finish_mutex);
            if (finish_count >= maxThread) {
                break;
            }
        }
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
