# 作业 2：从零构建任务执行库

**截止时间：10 月 16 日（星期四）晚 11:59**

**总分 100 分**

## 概述

人人都希望尽快完成任务，这次作业正是如此！你将实现一个 C++ 库，在多核 CPU 上尽可能高效地执行应用程序提供的任务。

第一部分要求实现任务执行库，支持批量启动同一种任务的多个实例，即数据并行执行。这与作业 1 中使用的 [ISPC 任务启动机制](http://ispc.github.io/ispc.html#task-parallelism-launch-and-sync-statements)类似，都是将代码并行分配到多个核心上执行。

第二部分要求扩展任务运行时系统，支持更复杂的**任务图**：某些任务的执行依赖其他任务产生的结果。这些依赖关系限制了调度系统能够安全并行执行的任务。在并行机器上调度数据并行任务图，是许多常用并行运行时系统的功能，包括 [Thread Building Blocks](https://github.com/intel/tbb)、[Apache Spark](https://spark.apache.org/)，以及 [PyTorch](https://pytorch.org/) 和 [TensorFlow](https://www.tensorflow.org/) 等现代深度学习框架。

本作业要求你：

* 使用线程池管理任务执行。
* 使用互斥锁、条件变量等同步原语协调工作线程。
* 实现遵循任务图依赖关系的任务调度器。
* 理解工作负载的特征，作出高效的任务调度决策。

建议阅读 [C++ 同步教程](tutorial/README.md)，了解 C++ 标准库中的同步原语。也可以查看[测试用例说明](tests/)，了解你的库需要支持哪些工作负载。

### 等等，我是不是已经做过这个了？

你可能已经在 CS107 或 CS111 等课程中创建过线程池和任务执行库。不过，本作业提供了进一步理解这些系统的机会：你将实现多个任务执行库，其中有的不用线程池，有的使用不同类型的线程池。通过实现多种调度策略并比较它们在不同工作负载上的性能，你将更深入地理解设计并行系统时关键决策的影响。

## 环境配置

**本作业将在 Amazon AWS `c7g.4xlarge` 实例上评分。虚拟机配置步骤见[中文说明](cloud_readme.zh-CN.md)（[英文原文](https://github.com/stanford-cs149/asst2/blob/master/cloud_readme.md)）。请确保代码能在该虚拟机上运行；性能测试和评分都将在这一环境中进行。**

起始代码位于 [GitHub](https://github.com/stanford-cs149/asst2)，可以从以下地址下载：


    https://github.com/stanford-cs149/asst2/archive/refs/heads/master.zip

**重要：不要修改提供的 `Makefile`，否则可能导致评分脚本失效。**

## A 部分：同步批量任务启动

在作业 1 中，你使用 ISPC 的任务启动原语启动了 N 个 ISPC 任务实例（`launch[N] myISPCFunction()`）。本作业第一部分要求在你的任务执行库中实现类似的功能。

首先熟悉 `itasksys.h` 中 `ITaskSystem` 的定义。这个[抽象类](https://www.tutorialspoint.com/cplusplus/cpp_interfaces.htm)定义了任务执行系统的接口，其中 `run()` 方法的签名为：


    virtual void run(IRunnable* runnable, int num_total_tasks) = 0;

`run()` 执行指定任务的 `num_total_tasks` 个实例。由于一次函数调用会执行许多任务，我们把每次 `run()` 调用称为一次**批量任务启动**。

`tasksys.cpp` 的起始代码提供了正确的串行实现 `TaskSystemSerial::run()`，展示任务系统如何通过 `IRunnable` 接口执行一批任务。`IRunnable` 定义在 `itasksys.h` 中。注意，每次调用 `IRunnable::runTask()` 时，任务系统都会传入当前任务的标识符（从 0 开始、在本次批量启动范围内的整数），以及本批任务总数。任务实现用这些参数决定自身应执行的工作。

`run()` 的一个重要细节是：任务执行相对于调用线程必须是同步的。也就是说，当 `run()` 返回时，应用程序必须能够确定本次批量启动的**全部任务**已经完成。起始代码中的串行实现直接在调用线程上完成所有任务，因此满足这一要求。

### 运行测试

起始代码包含一组使用任务系统的测试程序。测试说明见 `tests/README.md`，具体定义见 `tests/tests.h`。使用 `runtasks` 脚本运行测试。例如，`mandelbrot_chunked` 使用批量启动计算 Mandelbrot 分形图像，每个任务处理图像中连续的一块区域，运行命令为：

```bash
./runtasks -n 16 mandelbrot_chunked
```

不同测试的性能特征各不相同：有的任务计算量很小，有的很大；有的每批启动大量任务，有的只有少数任务；同一批任务的计算开销有时近似相同，有时差异很大。大部分测试在 `tests/README.md` 中有说明，但建议进一步查看 `tests/tests.h`，了解所有测试的行为。

> [!TIP]
> 实现过程中，可以用 `simple_test_sync` 调试正确性。这个测试很小，适合用打印语句或调试器排查问题，但不适合衡量性能。参见 `tests/tests.h` 中的 `simpleTest` 函数。

鼓励编写自己的测试。可以参考 `tests/tests.h` 中的现有测试，也可以使用其中由 `class YourTask` 和 `yourTest()` 函数组成的测试骨架。添加测试后，请将其加入 `tests/main.cpp` 中的测试列表和测试名称列表，并相应调整 `n_tests`。自己的实现可以运行这些测试，但你无法重新编译参考实现来运行自定义测试。

命令行选项 `-n` 指定任务系统实现最多可使用的线程数。上例选择 `-n 16`，因为 AWS 实例的 CPU 有 16 个执行上下文。使用帮助选项 `-h` 可以查看完整测试列表。

`-i` 指定性能测量时测试的运行次数。为得到准确的测量，`./runtasks` 会多次运行测试，并记录其中的**最短**运行时间。通常默认值已经足够；增加次数可能提高测量准确性，但也会增加测试耗时。

我们还提供用于性能评分的测试工具：

```bash
>>> python3 ../tests/run_test_harness.py
```

它支持以下命令行参数：

```bash
>>> python3 run_test_harness.py -h
usage: run_test_harness.py [-h] [-n NUM_THREADS]
                           [-t TEST_NAMES [TEST_NAMES ...]] [-a]

Run task system performance tests

optional arguments:
  -h, --help            show this help message and exit
  -n NUM_THREADS, --num_threads NUM_THREADS
                        Max number of threads that the task system can use. (16
                        by default)
  -t TEST_NAMES [TEST_NAMES ...], --test_names TEST_NAMES [TEST_NAMES ...]
                        List of tests to run
  -a, --run_async       Run async tests
```

输出的详细性能报告如下：

```bash
>>> python3 ../tests/run_test_harness.py -t super_light super_super_light
python3 ../tests/run_test_harness.py -t super_light super_super_light
================================================================================
Running task system grading harness... (2 total tests)
  - Detected CPU with 16 execution contexts
  - Task system configured to use at most 16 threads
================================================================================
================================================================================
Executing test: super_super_light...
Reference binary: ./runtasks_ref_linux
Results for: super_super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                9.053     9.022       1.00  (OK)
[Parallel + Always Spawn]               8.982     33.953      0.26  (OK)
[Parallel + Thread Pool + Spin]         8.942     12.095      0.74  (OK)
[Parallel + Thread Pool + Sleep]        8.97      8.849       1.01  (OK)
================================================================================
Executing test: super_light...
Reference binary: ./runtasks_ref_linux
Results for: super_light
                                        STUDENT   REFERENCE   PERF?
[Serial]                                68.525    68.03       1.01  (OK)
[Parallel + Always Spawn]               68.178    40.677      1.68  (NOT OK)
[Parallel + Thread Pool + Spin]         67.676    25.244      2.68  (NOT OK)
[Parallel + Thread Pool + Sleep]        68.464    20.588      3.33  (NOT OK)
================================================================================
Overall performance results
[Serial]                                : All passed Perf
[Parallel + Always Spawn]               : Perf did not pass all tests
[Parallel + Thread Pool + Spin]         : Perf did not pass all tests
[Parallel + Thread Pool + Sleep]        : Perf did not pass all tests
```

其中 `PERF` 是你的实现运行时间与参考实现运行时间的比值。因此，小于 1 表示你的任务系统比参考实现更快。

> [!TIP]
> Mac 用户：虽然 A、B 两部分都提供了 Mac 参考程序，但评分将使用 Linux 程序，因此建议提交前在 AWS 实例上检查实现。M1 等较新的 Mac 本地测试使用 `runtasks_ref_osx_arm`，其他 Mac 使用 `runtasks_ref_osx_x86`。

> [!IMPORTANT]
> 我们会在 AWS 上使用 ARM 版参考程序 `runtasks_ref_linux_arm` 评分。请确认你的实现能在 AWS ARM 实例上正确运行。

### 需要完成的工作

你的任务是实现一个能高效利用多核 CPU 的任务执行引擎。评分同时考虑正确性（必须正确执行全部任务）和性能。这是一个有趣但并不简单的编程挑战。为帮助你逐步推进，A 部分要求实现多个版本，逐步增加复杂度并提升性能。三个实现对应 `tasksys.cpp/.h` 中定义的类：

* `TaskSystemParallelSpawn`
* `TaskSystemParallelThreadPoolSpinning`
* `TaskSystemParallelThreadPoolSleeping`

**请在 `part_a/` 子目录中完成 A 部分，以便与对应参考实现 `part_a/runtasks_ref_*` 比较。**

小建议：下面的步骤采用“先尝试最简单的改进”的思路。每一步都会增加实现的复杂度，但在整个过程中，每完成一步都应拥有一个能够工作、完全正确的任务运行时系统。

此外，你至少需要编写一个测试，用于检查正确性或性能，详见上面的“运行测试”。

#### 第 1 步：改为并行任务系统

**本步骤实现 `TaskSystemParallelSpawn` 类。**

起始代码中的 `TaskSystemSerial` 是可用的串行实现。本步骤要求扩展起始代码，使一次批量启动中的任务能够并行执行。

* 需要创建额外线程来执行这批任务。`TaskSystem` 构造函数的参数 `num_threads` 是实现执行任务时允许使用的**最大工作线程数**。
* 按照“先做最简单的实现”的思路，建议在 `run()` 开始时创建工作线程，并在返回前由主线程对这些线程调用 join。这样能够保证正确性，但频繁创建线程会产生较大开销。
* 你会如何向工作线程分配任务？应采用静态分配还是动态分配？
* 是否存在共享变量（任务系统的内部状态），需要防止多个线程同时访问？

#### 第 2 步：使用线程池避免频繁创建线程

**本步骤实现 `TaskSystemParallelThreadPoolSpinning` 类。**

第 1 步的实现每次调用 `run()` 都要创建线程，任务计算开销很小时，这一开销尤其明显。建议改用线程池：预先创建所有工作线程，例如在构造 `TaskSystem` 时，或首次调用 `run()` 时创建。

* 初始实现可以让工作线程持续循环，检查是否还有工作可做。线程进入循环并等待某个条件成立，通常称为“自旋”（spinning）。工作线程应如何判断有工作可执行？
* 此时，要确保 `run()` 满足同步语义就不再那么简单。你需要怎样修改 `run()`，才能判断一批任务已经全部完成？

#### 第 3 步：没有工作时让线程休眠

**本步骤实现 `TaskSystemParallelThreadPoolSleeping` 类。**

第 2 步的缺点是，线程自旋等待时仍占用 CPU 核心的执行资源。例如，工作线程可能循环等待新任务；主线程也可能循环等待所有工作线程完成任务，以便从 `run()` 返回。这些线程没有执行有用工作，却仍消耗 CPU 资源，因而可能降低性能。

本步骤要求让线程在等待条件满足期间休眠，提高任务系统的效率。

* 可以使用条件变量。它能让线程在等待某个条件时休眠，不占用 CPU 计算资源。其他线程发出通知，唤醒等待线程，让它检查条件是否满足。例如，没有工作时让工作线程休眠，避免与实际执行任务的线程争抢 CPU；调用 `run()` 的主线程也可以在等待一批任务完成期间休眠，否则主线程自旋会抢占工作线程的 CPU 资源。
* 这一实现可能存在难以处理的竞态条件。需要考虑各种可能的线程执行交错。
* 建议增加测试用例。**起始代码包含性能评分所使用的工作负载，但我们还会用未提供的、更广泛的工作负载测试实现的正确性！**

## B 部分：支持任务图执行

B 部分要求扩展 A 部分的任务系统，支持异步启动任务，并允许任务依赖先前的任务。这些依赖会形成调度约束，任务执行库必须遵守。

`ITaskSystem` 接口还有一个方法：


    virtual TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                    const std::vector<TaskID>& deps) = 0;

与 `run()` 一样，`runAsyncWithDeps()` 也批量启动 `num_total_tasks` 个任务，但有以下区别。

#### 异步任务启动

首先，`runAsyncWithDeps()` 创建的任务相对于调用线程**异步**执行。这意味着，即使任务尚未完成，`runAsyncWithDeps()` 也应该**立即**返回。返回值是本次批量任务启动的唯一标识符。

调用线程可以通过 `sync()` 确定任务何时真正完成：


    virtual void sync() = 0;

**只有先前所有批量启动所关联的任务全部完成后，`sync()` 才能返回。**例如：


    // assume taskA and taskB are valid instances of IRunnable...

    std::vector<TaskID> noDeps;  // empty vector

    ITaskSystem *t = new TaskSystem(num_threads);

    // bulk launch of 4 tasks
    TaskID launchA = t->runAsyncWithDeps(taskA, 4, noDeps);

    // bulk launch of 8 tasks
    TaskID launchB = t->runAsyncWithDeps(taskB, 8, noDeps);

    // at this point tasks associated with launchA and launchB
    // may still be running

    t->sync();

    // at this point all 12 tasks associated with launchA and launchB
    // are guaranteed to have terminated

如注释所示，调用线程只有通过 `sync()` 同步后，才能确定之前 `runAsyncWithDeps()` 启动的任务已经完成。更准确地说，`runAsyncWithDeps()` 通知系统启动一批新任务，而实现可以灵活安排执行时间，只要在接下来的 `sync()` 返回前完成即可。这一规范并不保证先执行 `launchA` 的任务，再开始执行 `launchB` 的任务！

#### 支持显式依赖

第二个关键点是第三个参数：一个 `TaskID` 向量，其中的标识符必须来自此前 `runAsyncWithDeps()` 的批量启动。它指定本批任务依赖哪些先前启动的任务。**只有依赖向量中所列各批任务全部完成后，运行时才能开始执行当前批次中的任何任务。**例如：


    std::vector<TaskID> noDeps;  // empty vector
    std::vector<TaskID> depOnA;
    std::vector<TaskID> depOnBC;

    ITaskSystem *t = new TaskSystem(num_threads);

    TaskID launchA = t->runAsyncWithDeps(taskA, 128, noDeps);
    depOnA.push_back(launchA);

    TaskID launchB = t->runAsyncWithDeps(taskB, 2, depOnA);
    TaskID launchC = t->runAsyncWithDeps(taskC, 6, depOnA);
    depOnBC.push_back(launchB);
    depOnBC.push_back(launchC);

    TaskID launchD = t->runAsyncWithDeps(taskD, 32, depOnBC);
    t->sync();

上述代码有四次批量启动：taskA 有 128 个任务，taskB 有 2 个，taskC 有 6 个，taskD 有 32 个。taskB 和 taskC 都依赖 taskA，`launchD` 则依赖 `launchB` 和 `launchC` 的结果。因此，运行时可以以任意顺序处理 `launchB` 和 `launchC` 的任务，包括并行执行；但必须等 `launchA` 全部完成后才能开始，而且 B、C 都必须全部完成后，才允许执行 `launchD` 中的任何任务。

这些依赖可以表示为**任务图**。任务图是有向无环图（DAG）：节点对应批量任务启动，从 X 指向 Y 的边表示 Y 依赖 X 的输出。上述代码对应的任务图如下：

<p align="center">
    <img src="figs/task_graph.png" width=400>
</p>

如果在有 8 个执行上下文的 Myth 机器上运行这个例子，并行调度 `launchB` 和 `launchC` 会很有用，因为单独任何一批都不足以用满机器的执行资源。

### 测试

所有带 `Async` 后缀的测试都可用于 B 部分。评分工具包含的测试子集见 `tests/README.md`，全部测试定义在 `tests/tests.h`，并列于 `tests/main.cpp`。我们提供了小型测试 `simple_test_async` 方便调试正确性，参见 `tests/tests.h` 的 `simpleTest` 函数。它足够小，适合在 `simpleTest` 内使用打印语句或断点调试。

鼓励编写自己的测试。可以参考 `tests/tests.h` 中的测试，也可以使用 `class YourTask` 和 `yourTest()` 提供的测试骨架。将自定义测试加入 `tests/main.cpp` 中的测试列表和名称列表，并调整 `n_tests`。你可以用自己的实现运行自定义测试，但无法重新编译参考实现来运行这些测试。

### 需要完成的工作

扩展 A 部分使用休眠线程池的实现，正确实现 `TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps()` 和 `TaskSystemParallelThreadPoolSleeping::sync()`。还需要至少编写一个正确性或性能测试，详见“测试”。明确说明：你**需要**在报告中描述自定义测试，但自动评分器**不会**运行你编写的测试。**B 部分不需要实现其他 `TaskSystem` 类。**

以下建议可帮助你入手：

* 可以把 `runAsyncWithDeps()` 理解为向“工作队列”加入一条批量启动记录，或加入该批中每个任务各自的记录。工作记录入队后，方法即可返回。
* 本部分的关键是维护依赖关系所需的状态。一批任务全部完成时，应更新哪些状态？这正是其他新任务可能变为可执行状态的时刻。
* 可以维护两个数据结构：一是通过 `runAsyncWithDeps()` 加入系统、但因依赖任务尚未完成而不能执行的等待任务；二是没有未完成依赖、只要有空闲工作线程就可安全执行的任务“就绪队列”。
* 生成唯一启动标识符时，无须考虑整数溢出。测试不会发起超过 2^31 次批量启动。
* 可以假定程序只调用 `run()`，或只调用 `runAsyncWithDeps()`；无须处理 `run()` 等待此前异步启动全部完成的混合调用情况。基于这一假设，可以用适当的 `runAsyncWithDeps()` 和 `sync()` 调用实现 `run()`。
* 可以假定所有多线程行为都来自你的实现所创建或使用的线程。我们不会额外创建线程，再从这些线程调用你的实现。

**请在 `part_b/` 子目录中完成 B 部分，以便与对应参考实现 `part_b/runtasks_ref_*` 比较。**

## 评分

分值分配如下。

**A 部分（50 分）**

* `TaskSystemParallelSpawn::run()`：正确性 5 分，性能 5 分，共 10 分。
* `TaskSystemParallelThreadPoolSpinning::run()` 和 `TaskSystemParallelThreadPoolSleeping::run()`：每个方法正确性 10 分、性能 10 分，共 40 分。

**B 部分（40 分）**

* `TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps()`、`run()` 和 `sync()`：正确性共 30 分。
* 上述三个方法的性能共 10 分。B 部分可忽略 `Parallel + Always Spawn` 和 `Parallel + Thread Pool + Spin` 的结果，只需通过每个用例的 `Parallel + Thread Pool + Sleep` 检查。

**报告（10 分）**

* 详见“提交”。

对每个测试，运行时间不超过参考实现的 120%（A 部分）或 150%（B 部分），即可获得全部性能分。只有结果正确的实现才有性能分。如前所述，我们也可能使用未在起始代码中提供的、更广泛的工作负载测试正确性。

## 提交

请通过 [Gradescope](https://www.gradescope.com/) 提交任务系统代码和实现报告。需要提交以下五个文件：

* `part_a/tasksys.cpp`
* `part_a/tasksys.h`
* `part_b/tasksys.cpp`
* `part_b/tasksys.h`
* 报告 PDF，提交到 Gradescope 的报告作业项。

#### 代码提交

将 `part_a/tasksys.cpp|.h` 和 `part_b/tasksys.cpp|.h` 打包提交。可以创建 `asst2_submission` 目录，内建 `part_a`、`part_b` 子目录，放入对应文件后执行 `tar -czvf asst2.tar.gz asst2_submission`。将**压缩文件** `asst2.tar.gz` 上传到 Gradescope 的 *Assignment 2 (Code)*。

提交前确认所有代码都能编译、运行！我们应当能够将这些文件放入一份干净的起始代码目录，执行 `make` 后直接运行程序，无须人工干预。

评分脚本会运行起始代码中提供的检查程序来确定性能分。**我们还会使用未提供的其他应用程序进一步测试正确性！**评分脚本将在截止时间之后运行。

#### 报告提交

请向 Gradescope 的 *Assignment 2 (Write-up)* 提交一份简短报告，回答以下问题：

1. 描述任务系统实现，一页即可。除总体工作原理外，请说明线程如何管理（例如是否使用线程池）、任务如何分配给工作线程（静态还是动态），以及 B 部分如何跟踪依赖并确保任务图正确执行。
2. A 部分中，较简单的实现（完全串行，或每批任务都创建线程）有时与复杂实现性能相当，甚至更快。请结合具体测试解释原因：串行实现在哪些情况下最好，为什么？每批创建线程的实现在什么情况下能与线程池实现相当，在什么情况下不能？
3. 描述你为本作业编写的一个测试。它执行什么工作、检查什么、如何验证你的实现通过了测试且表现良好？这个测试的结果是否促使你修改实现？
