
#include <uth.h>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <sys/time.h>

using namespace madi;

/*
static inline double now(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec*1e-6;
}
*/

static long fib(long n)
{
    if (n < 2) {
        return n;
    } else {
        madm::future<long> f(fib, n - 1);

        long r1 = fib(n - 2);

        long r0 = f.touch();

        return r0 + r1;
    }
}

extern "C" long bin(long depth)
{
    if (depth == 0) {
        return 1;
    } else {
        madm::future<long> f(bin, depth - 1);

        long r1 = bin(depth - 1);

        long r0 = f.touch();

        return r0 + r1 + 1;
    }
}

extern "C" long bin2(long depth)
{
    if (depth == 0) {
        for (volatile int i = 0; i < 100 * 1000 * 1000; i++)
            ;
    } else {
        madm::future<long> f(bin2, depth - 1);
        bin2(depth - 1);
        f.touch();
    }
    return 0;
}

long identity(long depth)
{
    MADI_DPUTS("");
    return depth;
}


template <class F>
long perform_exp(madm::pid_t me, size_t n_procs, F f, long n, double t[4])
{
    long result0 = 0, result1 = 0;

    madm::barrier();

    t[0] = now();

    if (me == 0) {
        madm::future<long> f0(f, n);
        result0 = f0.touch();
    }

    t[1] = now();

    madm::barrier();

    t[2] = now();


    if (me == 0) {
        madm::future<long> f1(f, n);
        result1 = f1.touch();
    }

    t[3] = now();

    madm::barrier();

    if (me == 0) {
        long correct = (1L << (n + 1)) - 1;
        if (result0 != correct)
            MADI_DPUTS("result0 is incorrect: %ld (correct: %ld)",
                       result0, correct);
        if (result1 != correct)
            MADI_DPUTS("result1 is incorrect: %ld (correct: %ld)",
                       result1, correct);
    }

    return result0;
}

extern "C"
long test(long n)
{
    madm::future<long> f(bin, n);

    MADI_DPUTS("a task spawned");

    long result = f.touch();

    MADI_DPUTS("a task touched");

    return result;
}

void real_main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s N\n", argv[0]);
        madi::exit(1);
    }
    long n = atol(argv[1]);

    madm::pid_t me = madm::get_pid();
    MADI_UNUSED size_t n_procs = madm::get_n_procs();

    double t[4];

#define APP 0
#if APP == 0
    long result = perform_exp(me, n_procs, bin, n, t);
#elif APP == 1
    long result = perform_exp(me, n_procs, fib, n, t);
#elif APP == 2
    if (me == 0) {
        t[0] = now();
        madm::future<long> f(bin2, n);
        f.touch();
        t[1] = now();
    }
    madm::barrier();
#elif APP == 3
    if (me == 0) {
        madm::future<long> f(identity, n);
        f.touch();
    }
    madm::barrier();
#else
#endif

    if (me == 0) {
#if APP == 0
        printf("bin(%ld)    = %ld\n", n, result);
        printf("throughput  = %.3f\n", (double)result / (double)(t[1] - t[0]));
        printf("overhead    = %.9f\n", (double)(t[1] - t[0]) / (double)result);
#elif APP == 1
        printf("fib(%ld)    = %ld\n", n, result);
#endif
        printf("1st time    = %.6f\n", t[1] - t[0]);
        printf("2nd time    = %.6f\n", t[3] - t[2]);
    }
}

int main(int argc, char **argv) {
    madm::env env(argc, argv);

    env.start(real_main, argc, argv);

    return 0;
}
