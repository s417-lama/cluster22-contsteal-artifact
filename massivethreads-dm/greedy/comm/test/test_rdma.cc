#include <madm_comm.h>
#include <madm_debug.h>
#include <cstdio>
#include <cstring>
#include <new>

using namespace madi;

int real_main(int argc, char **argv)
{
    pid_t me = comm::get_pid();
    size_t n_procs = comm::get_n_procs();

    pid_t prev = (me - 1 + n_procs) % n_procs;
    pid_t next = (me + 1) % n_procs;

    pid_t prev_prev = (prev - 1 + n_procs) % n_procs;

    MADI_DPUTS("me = %5d, n_procs = %5zu", me, n_procs);
    MADI_DPUTS2("prev_prev = %5d, prev = %5d, next = %5d",
               prev_prev, prev, next);

    size_t n_elems = 4096;
    int **remote_ptrs = comm::coll_rma_malloc<int>(n_elems);

    for (size_t i = 0; i < n_elems; i++)
        remote_ptrs[me][i] = 32;

    comm::barrier();

#if 1
    int value = 1000 + me;

    MADI_DPUTS2("");
    MADI_DPUTSR2("PUT: remote_ptrs[%d] + %d = %d",
                next, next, value);

    comm::put_value<int>(remote_ptrs[next] + next, value, next);
#endif

    comm::barrier();

#if 1
    for (size_t i = 0; i < n_procs; i++) {
        MADI_DPUTS2("");

        int v = comm::get_value<int>(remote_ptrs[prev] + i, prev);

        MADI_DPUTSR2("GET: remote_ptrs[%d] + %d = %d", prev, i, v);

        if (i == prev)
            MADI_ASSERT(v == 1000 + prev_prev);
        else
            MADI_ASSERT(v == 32);
    }
#else
    for (size_t i = 0; i < 2; i++) {
        int v = remote_ptrs[me][me + i];

        MADI_DPUTS2("");
        MADI_DPUTS2("RAW: remote_ptrs[%d] + %d + %d = %d", me, me, i, v);

        //MADI_ASSERT(v == prev_prev);
    }
#endif
    MADI_DPUTS2("");

    comm::coll_rma_free(remote_ptrs);

    return 0;
}

int main(int argc, char **argv)
{
    comm::initialize(argc, argv);

    pid_t me = comm::get_pid();
    size_t n_procs = comm::get_n_procs();

    MADI_DPUTS3("me = %5d, n_procs = %5zu", me, n_procs);

#if 1
    comm::start(real_main, argc, argv);
#else
    real_main(argc, argv);
    comm::barrier();
#endif

    comm::finalize();
}

