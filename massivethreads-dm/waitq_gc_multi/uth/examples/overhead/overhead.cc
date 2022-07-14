#include <uth.h>

#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>

namespace uth = madm::uth;


int do_nothing(int i)
{
    return i;
}

int do_nothing_noinline(int i);

int measure()
{
    size_t times = 3e7;
#if 0
    // warmup
    for (auto i = 0 ; i < times; i++) {
        uth::thread<int>(do_nothing, 0).join();
    }
#endif
    {
        long t0 = uth::tick();

        for (auto i = 0 ; i < times; i++)
            uth::thread<int>(do_nothing, 0).join();

        long t1 = uth::tick();

        printf("tasking overhead = %ld\n", (t1 - t0) / times);
    }

    {
        long t0 = uth::tick();

        for (auto i = 0; i < times; i++)
            uth::thread<int>([=]() { return i; }).join();

        long t1 = uth::tick();

        printf("tasking overhead (lambda) = %ld\n", (t1 - t0) / times);
    }

    {
        long t0 = uth::tick();

        for (auto i = 0; i < times; i++)
            do_nothing_noinline(0);

        long t1 = uth::tick();

        printf("function call overhead = %ld\n", (t1 - t0) / times);
    }

    return 0;
}

void real_main(int argc, char **argv)
{
    uth::thread<int>(measure).join();
}

int main(int argc, char **argv)
{
    uth::start(real_main, argc, argv);
    return 0;
}

