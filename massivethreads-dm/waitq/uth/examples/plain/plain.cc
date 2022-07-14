#include <uth.h>

#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>

namespace uth = madm::uth;

double t0, t1, t2;

void real_main(int argc, char **argv)
{
    MADI_DPUTS("start");

    t1 = uth::time();

    uth::barrier();

    t2 = uth::time();

    auto me = uth::get_pid();
    auto n_procs = uth::get_n_procs();

    if (me == 0) {
        printf("n_procs = %5zu, init = %9.6f, barrier = %9.6f, ",
               n_procs, t1 - t0, t2 - t1);
    }

}

int main(int argc, char **argv)
{
    t0 = uth::time();

    uth::start(real_main, argc, argv);

    return 0;
}

