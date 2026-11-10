#include "tasksys.h"

#include <algorithm>

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
    curTask = 0;
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
                            {
                                std::lock_guard<std::mutex> lock(
                                    finished_task_mutex);
                                if (pending_task_count == 0) {
                                    finished_task_cv.notify_all();
                                }
                            }
                            break;
                        }
                    }
                    task.runnable->runTask(task.task_idx, task.num_total_tasks);
                    bool batch_finished;
                    {
                        std::lock_guard<std::mutex> lock(task_mutex);
                        batch_finished = --task_remaining[task.task_id] == 0;
                    }
                    if (batch_finished) {
                        {
                            std::lock_guard<std::mutex> lock(set_mutex);
                            finished_tasks.insert(task.task_id);
                        }
                        {
                            std::lock_guard<std::mutex> lock(pending_set_mutex);
                            for (auto it = pending_tasks.begin();
                                 it != pending_tasks.end();) {
                                if (insertingTask(*it))
                                    it = pending_tasks.erase(it);
                                else
                                    ++it;
                            }
                        }
                    }
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

bool TaskSystemParallelThreadPoolSleeping::insertingTask(PendingTask& task) {
    auto& runnable = task.runnable;
    auto& num_total_tasks = task.num_total_tasks;
    auto& deps = task.deps;
    auto& task_id = task.task_id;
    bool deps_completed = true;
    for (auto dep : deps) {
        std::lock_guard<std::mutex> lock(set_mutex);
        if (!finished_tasks.count(dep)) {
            deps_completed = false;
            break;
        }
    }
    if (deps_completed) {
        if (num_total_tasks == 0) {
            {
                std::lock_guard<std::mutex> lock(set_mutex);
                finished_tasks.insert(task_id);
            }
            {
                std::lock_guard<std::mutex> lock(finished_task_mutex);
                if (--pending_task_count == 0) {
                    finished_task_cv.notify_all();
                }
            }
            return true;
        }
        for (int i = 0; i < num_total_tasks; i++) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push_back(Task{runnable, i, num_total_tasks, task_id});
        }
        for (int i = 0; i < num_threads; i++) {
            std::unique_lock<std::mutex> lock(status_mutex[i]);
            status_call[i] = 1;
            cv[i].notify_all();
        }
        return true;
    }
    return false;
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
    TaskID task_id;
    {
        std::lock_guard<std::mutex> lock(task_mutex);
        task_id = curTask++;
        task_remaining.push_back(num_total_tasks);
    }

    {
        std::lock_guard<std::mutex> lock(finished_task_mutex);
        pending_task_count += num_total_tasks == 0 ? 1 : num_total_tasks;
    }
    std::unique_lock<std::mutex> lockP(pending_set_mutex);
    PendingTask task{runnable, num_total_tasks, deps, task_id};
    if (!insertingTask(task)) {
        pending_tasks.push_back(task);
    }

    return task_id;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
    std::unique_lock<std::mutex> lock(finished_task_mutex);
    finished_task_cv.wait(lock, [&] { return pending_task_count == 0; });
    return;
}
