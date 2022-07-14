#include <uth.h>

#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>

namespace uth = madm::uth;


int do_task(int n)
{
    long t = uth::tick();

    while (uth::tick() - t < n * 1000)
        uth::poll();

    return n;
}

int measure()
{
    size_t times = 1000;
#if 0
    // warmup
    for (auto i = 0 ; i < times; i++)
        uth::thread<int>(do_nothing, 0).join();
#endif
    long t0 = uth::tick();

    for (auto i = 0 ; i < times; i++)
        uth::thread<int>(do_task, 5000).join();

    long t1 = uth::tick();

    printf("cycles per task = %ld\n", (t1 - t0) / times);

    return 0;
}

void real_main(int argc, char **argv)
{
    uth::pid_t me = uth::get_pid();

    if (me == 0)
        uth::thread<int>(measure).join();

    uth::barrier();
}

int main(int argc, char **argv)
{
    uth::start(real_main, argc, argv);
    return 0;
}

