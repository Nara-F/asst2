#include "tasksys.h"

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
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

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
    : ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement
    // TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement
    // TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement
    // TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // NOTE: CS149 students are not expected to implement
    // TaskSystemParallelSpawn in Part B.
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
    : ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement
    // TaskSystemParallelThreadPoolSpinning in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable,
                                               int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement
    // TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement
    // TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // NOTE: CS149 students are not expected to implement
    // TaskSystemParallelThreadPoolSpinning in Part B.
    return;
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
      status_mutex(std::max(num_threads - 1, 1)),
      cv(std::max(num_threads - 1, 1)) {
    this->num_threads =
        std::max(num_threads - 1, 1);  // preventing block of main thread
    threads.resize(this->num_threads);
    status_call.resize(this->num_threads, 0);

    auto wait = [&](int idx) {
        while (true) {
            std::unique_lock<std::mutex> lock(status_mutex[idx]);
            cv[idx].wait(lock, [&] {
                return status_call[idx] == 1 || status_call[idx] == -1;
            });
            auto sta = status_call[idx];
            lock.unlock();
            if (sta == 1) {
                Task task;
                do {
                    {
                        std::lock_guard<std::mutex> lock(queue_mutex);
                        if (!task_queue.empty()) {
                            task = task_queue.front();
                            task_queue.pop_front();
                        } else {
                            {
                                std::lock_guard<std::mutex> lock(
                                    status_mutex[idx]);
                                if (status_call[idx] == 1) status_call[idx] = 0;
                            }
                            break;
                        }
                    }
                    task.runnable->runTask(task.task_id, task.num_total_tasks);
                    {
                        std::lock_guard<std::mutex> lock(finished_task_mutex);
                        if (--pending_task_count == 0) {
                            finished_task_cv.notify_all();
                        }
                    }
                } while (true);
            } else if (sta == -1) {
                break;
            }
        }
    };

    for (int i = 0; i < this->num_threads; i++) {
        threads[i] = std::thread(wait, i);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    for (int i = 0; i < num_threads; i++) {
        std::unique_lock<std::mutex> lock(status_mutex[i]);
        status_call[i] = -1;
        cv[i].notify_all();
    }
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable,
                                               int num_total_tasks) {
    runAsyncWithDeps(runnable, num_total_tasks, {});
    sync();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) {
    //
    // TODO: CS149 students will implement this method in Part B.
    //
    bool deps_completed = true;
    for (auto dep : deps) {
        if (!finished_tasks.count(dep)) {
            deps_completed = false;
            break;
        }
    }
    {
        std::lock_guard<std::mutex> lock(finished_task_mutex);
        pending_task_count += num_total_tasks;
    }
    if (deps_completed) {
        for (int i = 0; i < num_total_tasks; i++) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push_front(Task{runnable, i, num_total_tasks});
        }
    } else {
        for (int i = 0; i < num_total_tasks; i++) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push_back(Task{runnable, i, num_total_tasks});
        }
    }
    for (int i = 0; i < num_threads; i++) {
        std::unique_lock<std::mutex> lock(status_mutex[i]);
        status_call[i] = 1;
        cv[i].notify_all();
    }

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
    std::unique_lock<std::mutex> lock(finished_task_mutex);
    finished_task_cv.wait(lock, [&] { return pending_task_count == 0; });
    return;
}
