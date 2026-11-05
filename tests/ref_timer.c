/* Repair the cached conversion factor in the supplied Linux x86-64 ref.
 * Loaded before main; the reference executable on disk is never changed.
 */
#define _GNU_SOURCE
#include <cpuid.h>
#include <errno.h>
#include <link.h>
#include <math.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>

static void fail(const char *message) {
    fprintf(stderr, "ref timer: %s\n", message);
    _exit(125);
}

static double now(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) fail("clock_gettime failed");
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static uint64_t ticks(void) {
    unsigned cpu;
    uint64_t value = __rdtscp(&cpu);
    _mm_lfence();
    return value;
}

static double sample(long nanoseconds) {
    double start = now();
    uint64_t first = ticks();
    struct timespec delay = {0, nanoseconds};
    while (nanosleep(&delay, &delay)) {
        if (errno != EINTR) fail("nanosleep failed");
    }
    uint64_t last = ticks();
    double elapsed = now() - start;
    if (last <= first || elapsed <= 0) fail("non-monotonic clock");
    return (last - first) / elapsed;
}

static uintptr_t offset(const char *name) {
    const char *value = getenv(name);
    char *end;
    if (!value || !*value) fail("missing symbol offset; use run_reference.py");
    errno = 0;
    unsigned long long result = strtoull(value, &end, 16);
    if (errno || *end || !result) fail("invalid symbol offset");
    return (uintptr_t)result;
}

struct symbols {
    uintptr_t scale, initialized, function, base;
};

static int contains(const ElfW(Phdr) *ph, uintptr_t address,
                    size_t size, unsigned flags) {
    return ph->p_type == PT_LOAD && (ph->p_flags & flags) == flags &&
           address >= ph->p_vaddr && size <= ph->p_memsz &&
           address - ph->p_vaddr <= ph->p_memsz - size;
}

static int locate(struct dl_phdr_info *info, size_t size, void *data) {
    (void)size;
    if (info->dlpi_name && *info->dlpi_name) return 0;
    struct symbols *s = data;
    int scale_ok = 0, init_ok = 0, function_ok = 0;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
        scale_ok |= contains(ph, s->scale, sizeof(double), PF_R | PF_W);
        init_ok |= contains(ph, s->initialized, 1, PF_R | PF_W);
        function_ok |= contains(ph, s->function, 1, PF_R | PF_X);
    }
    if (!scale_ok || !init_ok || !function_ok) fail("invalid reference symbols");
    s->base = info->dlpi_addr;
    return 1;
}

__attribute__((constructor)) static void calibrate(void) {
    if (!getenv("CS149_REF_TIMER_SCALE")) return;
    unsigned a, b, c, d;
    if (!__get_cpuid(0x80000001, &a, &b, &c, &d) || !(d & (1u << 27)) ||
        !__get_cpuid(0x80000007, &a, &b, &c, &d) || !(d & (1u << 8)))
        fail("an invariant TSC and RDTSCP are required");

    /* Use the kernel's TSC reliability decision, including synchronization
     * across cores. Ref workers may run on any CPU after calibration.
     */
    FILE *file = fopen("/sys/devices/system/clocksource/clocksource0/current_clocksource", "r");
    char clocksource[32];
    if (!file) fail("cannot check the kernel clocksource");
    int read_ok = fscanf(file, "%31s", clocksource) == 1;
    fclose(file);
    if (!read_ok || strcmp(clocksource, "tsc"))
        fail("kernel is not using TSC as a reliable clocksource");

    cpu_set_t original, single;
    if (sched_getaffinity(0, sizeof(original), &original)) fail("cannot read CPU affinity");
    int cpu = sched_getcpu();
    if (cpu < 0 || cpu >= CPU_SETSIZE) fail("cannot select calibration CPU");
    CPU_ZERO(&single);
    CPU_SET(cpu, &single);
    if (sched_setaffinity(0, sizeof(single), &single)) fail("cannot pin calibration CPU");

    double rates[5];
    for (int i = 0; i < 5; ++i) rates[i] = sample(20000000);
    for (int i = 1; i < 5; ++i)
        for (int j = i; j > 0 && rates[j] < rates[j - 1]; --j) {
            double tmp = rates[j]; rates[j] = rates[j - 1]; rates[j - 1] = tmp;
        }
    double hz = rates[2];
    if ((rates[4] - rates[0]) / hz > 0.005) fail("unstable calibration; retry on an idle machine");

    struct symbols s = {offset("CS149_REF_TIMER_SCALE"),
                        offset("CS149_REF_TIMER_INITIALIZED"),
                        offset("CS149_REF_TIMER_FUNCTION"), 0};
    if (!dl_iterate_phdr(locate, &s)) fail("cannot locate the reference executable");
    *(double *)(s.base + s.scale) = 1.0 / hz;
    *(unsigned char *)(s.base + s.initialized) = 1;

    /* Call the actual reference timer accessor, then verify its conversion
     * on a separate 50 ms interval. This checks the runtime patch too.
     */
    double (*seconds_per_tick)(void) = (void *)(s.base + s.function);
    double scale = seconds_per_tick();
    double error = fabs(sample(50000000) * scale - 1.0);
    if (!isfinite(error) || error > 0.005) fail("reference timer failed wall-time verification");
    if (sched_setaffinity(0, sizeof(original), &original)) fail("cannot restore CPU affinity");

    if (getenv("CS149_REF_TIMER_CHECK")) {
        fprintf(stderr, "ref timer: TSC %.3f MHz; independent wall-time error %.5f%% (limit 0.5%%); OK\n",
                hz / 1e6, error * 100);
        _exit(0);
    }
    unsetenv("CS149_REF_TIMER_SCALE");
    unsetenv("CS149_REF_TIMER_INITIALIZED");
    unsetenv("CS149_REF_TIMER_FUNCTION");
}
