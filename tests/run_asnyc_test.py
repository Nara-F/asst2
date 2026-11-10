import argparse
import platform
import re
import subprocess
import multiprocessing
from pathlib import Path
import sys

STUDENT_BINARY_NAME = "runtasks"

REFERENCE_BINARY_NAME = "runtasks_ref"
TASKSYS_DEFAULT_NUM_THREADS = multiprocessing.cpu_count()
UNSPECIFIED_NUM_THREADS = -1

PERF_THRESHOLD = 1.5
NUM_TEST_RUNS = 5

LIST_OF_TESTS = [
    ("super_super_light", UNSPECIFIED_NUM_THREADS),
    ("super_light", UNSPECIFIED_NUM_THREADS),
    ("ping_pong_equal", UNSPECIFIED_NUM_THREADS),
    ("ping_pong_unequal", UNSPECIFIED_NUM_THREADS),
    ("recursive_fibonacci", UNSPECIFIED_NUM_THREADS),
    ("math_operations_in_tight_for_loop", UNSPECIFIED_NUM_THREADS),
    ("math_operations_in_tight_for_loop_fewer_tasks", UNSPECIFIED_NUM_THREADS),
    ("math_operations_in_tight_for_loop_fan_in", UNSPECIFIED_NUM_THREADS),
    ("math_operations_in_tight_for_loop_reduction_tree", UNSPECIFIED_NUM_THREADS),
    ("spin_between_run_calls", UNSPECIFIED_NUM_THREADS),
    ("mandelbrot_chunked", UNSPECIFIED_NUM_THREADS),
]

LIST_OF_IMPLEMENTATIONS_ORIG = [
    "REFERENCE [Parallel + Thread Pool + Sleep]",
    "STUDENT [Parallel + Thread Pool + Sleep]",
]

AUTHORS = ["STUDENT", "REFERENCE"]

LIST_OF_IMPLEMENTATIONS = [
    "[Parallel + Thread Pool + Sleep]",
]


def run_test(cmd, is_reference):
    runtimes = {}
    try:
        output = subprocess.check_output(cmd, text=True)
        for line in output.split('\n'):
            m = re.match(r'\[(.*)\]:\s+\[(\d+\.\d+)\] ms', line)
            if m is not None:
                implementation = m.group(1)
                if "[" + implementation + "]" not in LIST_OF_IMPLEMENTATIONS:
                    continue
                if is_reference:
                    implementation = "REFERENCE [%s]" % implementation
                else:
                    implementation = "STUDENT [%s]" % implementation
                runtime = float(m.group(2))
                runtimes[implementation] = [runtime]
        expected = {("REFERENCE " if is_reference else "STUDENT ") + impl
                    for impl in LIST_OF_IMPLEMENTATIONS}
        if set(runtimes) != expected:
            raise RuntimeError("missing implementation timings")
    except (subprocess.SubprocessError, RuntimeError) as e:
        if isinstance(e, subprocess.CalledProcessError) and e.output:
            print(e.output, end='')
        raise SystemExit("%s benchmark failed: %s" %
                         ("REFERENCE" if is_reference else "STUDENT", e))
    return runtimes

def pretty_print(test_name, runtimes):
    print("Results for: %s" % test_name)
    for implementation in LIST_OF_IMPLEMENTATIONS_ORIG:
        if implementation in runtimes:
            print("%s\t%.3f" % (implementation, runtimes[implementation]))

def pretty_print_with_comparison(test_name, runtimes, perf_threshold, impl_perf_ok):
    print("Results for: %s" % test_name)
    
    print("%s%s%sPERF?" % (" " * 40, "{:<10}".format(AUTHORS[0]), "{:<12}".format(AUTHORS[1])))
    for impl in LIST_OF_IMPLEMENTATIONS:
        student_impl = AUTHORS[0] + " " + impl 
        ref_impl = AUTHORS[1] + " " + impl 
        student_time = runtimes[student_impl] if student_impl in runtimes else "Missing"
        ref_time = runtimes[ref_impl] if ref_impl in runtimes else "Missing"

        try:
            relative_perf = student_time / ref_time
        except:
            continue

        # Check the threshold
        perf_ok = relative_perf < perf_threshold
        feedback = "(OK)" if perf_ok else "(NOT OK)"

        if not perf_ok:
            impl_perf_ok[impl] = False

        print("{:<40}{:<10}{:<12}{:.2f}  {}".format(impl, student_time, ref_time, relative_perf, feedback))



if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='Run Part B async tests for the sleeping thread pool')
    
    parser.add_argument('-n', '--num_threads', type=int,
                        default=TASKSYS_DEFAULT_NUM_THREADS,
                        help="Max number of threads that the task system can use. (%d by default)" % TASKSYS_DEFAULT_NUM_THREADS)
    parser.add_argument('-t', '--test_names', type=str, nargs='+',
                        choices=[x[0] for x in LIST_OF_TESTS],
                        default=[x[0] for x in LIST_OF_TESTS],
                        help='List of tests to run: %s' % ", ".join([
                            x[0] for x in LIST_OF_TESTS]))
    parser.add_argument('-a', '--run_async', action='store_true',
                        help='Compatibility option; this script always runs async tests')
    parser.add_argument('--runs', type=int, default=NUM_TEST_RUNS,
                        help='Number of separate processes per test (default: 5)')
    parser.add_argument('--iterations', type=int, default=3,
                        help='Timing iterations in each process (default: 3)')
    parser.add_argument('--raw-reference', action='store_true',
                        help='Use the original uncalibrated reference timer (diagnostics only)')

    args = parser.parse_args()
    if args.num_threads < 1 or args.runs < 1 or args.iterations < 1:
        parser.error('threads, runs, and iterations must be positive')

    if platform.system() == 'Darwin':
        suffix = 'osx_arm' if platform.machine() == 'arm64' else 'osx_x86'
    else:
        suffix = 'linux_arm' if platform.machine() in ('arm64', 'aarch64') else 'linux'
    part_b_dir = Path(__file__).resolve().parent.parent / 'part_b'
    reference_binary = str(part_b_dir / ('%s_%s' % (REFERENCE_BINARY_NAME, suffix)))
    ref_cmd = [reference_binary]
    if platform.system() == 'Linux' and platform.machine() == 'x86_64' and not args.raw_reference:
        ref_cmd = [sys.executable, str(Path(__file__).resolve().with_name('run_reference.py')),
                   reference_binary]
        print('Reference timer: wall-time calibrated TSC; verified in every reference process', flush=True)
    print('Reference binary: %s' % reference_binary, flush=True)
    student_cmd = [str(part_b_dir / STUDENT_BINARY_NAME), '--sleep-only']

    test_names_and_num_threads = []

    # Some tests directly specify the number of threads the task system should use. 
    # Other tests should configure the task system to use the default number of threads 
    for x in LIST_OF_TESTS:
        if x[0] not in args.test_names:
            continue

        if x[1] == UNSPECIFIED_NUM_THREADS:
            num_threads = args.num_threads
        else:
            num_threads = x[1]
        test_names_and_num_threads.append((x[0] + "_async", num_threads))

    print("==============================================================="
          "=================")
    print("Running task system grading harness... (%d total tests)" % len(test_names_and_num_threads))
    print("  - Detected CPU with %d execution contexts" % multiprocessing.cpu_count())
    print("  - Task system configured to use at most %d threads" % args.num_threads)
    print("==============================================================="
          "=================")

    runtimes_of_test = {}
    impl_perf_ok = {impl: True for impl in LIST_OF_IMPLEMENTATIONS}

    # run all tests
    for (test_name, num_threads) in test_names_and_num_threads:
        
        print("==============================================================="
              "=================")
        print("Executing test: %s..." %  test_name)

        # Keep per-test thread counts supported by LIST_OF_TESTS.
        timing_args = ['-n', str(num_threads), '-i', str(args.iterations)]
        cmds = [ref_cmd + timing_args, student_cmd + timing_args]
        is_references = [True, False]
        all_runtimes = {}
        for i in range(args.runs):
            for (cmd, is_reference) in zip(cmds, is_references):
                runtimes = run_test(cmd + [test_name], is_reference=is_reference)
                for key in runtimes:
                    if key not in all_runtimes:
                        all_runtimes[key] = []
                    all_runtimes[key] += runtimes[key]
        for key in all_runtimes:
            all_runtimes[key] = min(all_runtimes[key])
        pretty_print_with_comparison(test_name, all_runtimes, PERF_THRESHOLD, impl_perf_ok)
        
        runtimes_of_test[test_name] = all_runtimes

    # Compare student's implementation against reference
    print("==============================================================="
          "=================")
    print("Overall performance results")
    for impl in LIST_OF_IMPLEMENTATIONS:
        final_feedback = "All passed Perf" if impl_perf_ok[impl] else "Perf did not pass all tests"
        print("{:<40}: {}".format(impl, final_feedback))

