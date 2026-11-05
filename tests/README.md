# Test Harness Readme #
This readme is a description of the tests included in the test harness (`run_test_harness.py`). There are additional tests defined in `tests.h` and listed in the `main` function of `main.cpp`. Those tests can be run directly through the
`../part_*/runtasks` binary.

## 本地 Linux x86-64 的可靠 ref 计时 ##

原始 x86 ref 使用 `rdtsc`，却按 `/proc/cpuinfo` 的动态 CPU MHz 换算秒数。
当前学生代码的 `CycleTimer.h` 已改用 `steady_clock`，两者直接比较可能严重失真。
本目录的 harness 现在默认通过 `run_reference.py` 启动 Linux x86-64 ref。
原始 ref 文件保持不变，直接执行 `./runtasks_ref_linux` 仍会使用旧计时器。

在 `part_a/` 或 `part_b/` 中运行：

```sh
# 检查当前机器上的校准与独立时间验证（无需编译学生代码）。
python3 ../tests/run_reference.py --check ./runtasks_ref_linux

# 单独运行校准后的 ref，原有 ref 参数照常传入。
python3 ../tests/run_reference.py ./runtasks_ref_linux -n 4 -i 3 super_light

# 正常比较：Linux x86-64 会自动校准 ref，默认每个测试跑 5 个进程，
# 每个进程测 3 次，最终取最短时间。
make
python3 ../tests/run_test_harness.py -n 4

# 快速运行指定测试。
python3 ../tests/run_test_harness.py -n 4 --runs 1 --iterations 2 \
    -t super_super_light super_light math_operations_in_tight_for_loop
```

首次运行需要 `gcc` 和 `nm`（binutils），会自动在忽略的 `.ref_timer/` 目录编译
一个小型 `LD_PRELOAD` 库。它通过 ELF 符号定位 ref 的计时器，在 `main()` 前：

1. 检查 invariant TSC、RDTSCP，以及 Linux 当前使用的 `tsc` clocksource。
2. 临时固定主线程的 CPU，用 `CLOCK_MONOTONIC_RAW` 测量 5 次 TSC 频率，取中位数。
3. 设置 ref 原有计时器缓存的换算系数，调用 ref 的真实计时器函数，
   再以独立的 50 ms 区间验证误差不超过 0.5%，恢复原有 CPU 亲和性后运行测试。

校准在每个 ref 进程启动时执行，不计入测试时间。符号不匹配、时钟不可靠或验证失败
都会以非零状态退出；harness 不会把失败或缺失的测量报告成通过。
`--raw-reference` 仅用于诊断时对照旧结果。

这是本地性能基准：ref 的调度算法、任务计算代码和编译器产物均保留，
因此修正计时后仍可能存在 GCC 版本导致的任务计算速度差异，不能保证串行结果相同。
120% / 150% 的阈值沿用原脚本，不能据此保证其他机器的评分。
测量时尽量保持电源模式一致、减少后台负载；可用 `taskset -c 0-3 python3
../tests/run_test_harness.py -n 4` 让两边都使用同一组 CPU（CPU 编号按本机选择）。
ARM / macOS 维持原来的启动方式，此修复只用于 Linux x86-64。

验证修复：在仓库根目录运行
`python3 -m unittest discover -s tests -p 'test_reference_timer.py'`。

## SuperSuperLight ##
This test allocates two buffers of size 2^15 elements each and then simply copies elements from one buffer to the other buffer, reversing the order of the copy in each iteration. There are 64 tasks and 400 bulk task launches.

## SuperLight ##
This test allocates two buffers of size 2^15 elements each, and copies elements from one buffer to another in alternating order. Each task will increment values in the input buffer before copying the data to the output buffer. There are 64 tasks and 400 bulk task launches.

## PingPongEqual ##
This test allocates two buffers of size 2^19 elements each and copies elements from one buffer to another in alternating order. Each task will increment values in the input buffer before copying the data to the output buffer. The distribution of work across tasks is made to be equal. There are 64 tasks and 400 bulk task launches.

## PingPongUnequal ##
This test is the same as `pingPongEqual`, except it assigns more work to some tasks than others.

## RecursiveFibonacci ##
This test computes a Fibonacci number using a very compute-intensive recursive method. The computation is specified with 30 bulk task launches, each with 256 tasks. 

## MathOperationsInTightForLoop ##
Each task in this test takes no input and performs 32 compute-intensive computations involving exponent, logarithm, multiply, and add operations. The result of each computation is written into its own index in an output array. There are 16 tasks per bulk launch, the output array for each bulk launch is size 512, and there are 2000 bulk task launches. For the test with dependencies, each task depends on the previous task.

## MathOperationsInTightForLoopFewerTasks ##
This test is the same as `MathOperationsInTightForLoop`, except it splits up the 512 pieces of work per bulk launch among only 9 tasks. Therefore, each task gets a larger share of the computation, and some tasks get slightly more work than others.

## MathOperationsInTightForLoopFanIn ##
This test is split into two types of tasks. First is a set of compute-intensive math operations of the same type as `MathOperationsInTightForLoop`. The second is a single add-reduce operation, which occurs at the end. The reduce depends on all of the compute-intensive math tasks. For each bulk launch of math operations, there are 64 tasks writing to an array of size 2048, and there are 256 bulk launches. The reduce operation does a vector sum of the output vectors from all bulk launches, so the input of the reduce is a vector of size 256*2048 and the output is a vector of size 2048.

## MathOperationsInTightForLoopReductionTree ##
This test is similar to `MathOperationsInTightForLoopFanIn`, except instead of a single add-reduce, it does add-reduce operations in a binary-tree structure, where each add-reduce sums the results of two bulk task launches. Each add-reduce operation is dependent on two preceding operations, which are either both element-wise math or both add-reduce. For the math operations, there are 64 tasks per bulk launch writing to an array of size 16384, and there are 32 bulk launches. Each add-reduce takes an input array of size 32768 and outputs an array of size 16384.

## SpinBetweenRunCalls ##
First, spawns one bulk task launch of a single lightweight task that simply copies a single value to an output array. Second, spawns a launch of 2 medium-weight tasks that each compute the 40th Fibonacci number using the recursive method. Third, spawns another launch of a single lightweight task. In the async case, the final task depends on the first two.

## MandelbrotChunked ##
This test uses 128 tasks in a single bulk task launch to compute a [Mandelbrot fractal](https://en.wikipedia.org/wiki/Mandelbrot_set) image by decomposing the problem into tasks that produce contiguous chunks of output image rows. The input to each task is a specification of the view window and specifics of the Mandelbrot fractal algorithm. The output is an array containing the Mandelbrot fractal image. The computation itself is compute-intensive. Note that, because only one bulk task launch is performed, thread pool and spawning threads each run() should have similar performance.
