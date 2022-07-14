#include <cstring>
#include <cmath>
#include <cassert>
#include <unistd.h>
#include <vector>
#include <madm_comm.h>
#include <madm_debug.h>

using namespace std;
using namespace madi;

static int debug = 0;
static FILE *g_fp = NULL;


struct timebuf {
    size_t target;
    size_t size;
    tsc_t comm;
    tsc_t fence;
};

#if 1
template <class T>
void coll_allocate(size_t size, T ***ptrs, int *id)
{
    *ptrs = comm::coll_rma_malloc<T>(size);
    *id = 0;
}

void put(void *dst, void *src, size_t size, int target, int id)
{
    comm::put(dst, src, size, target);
}

void get(void *dst, void *src, size_t size, int target, int id)
{
    comm::get(dst, src, size, target);
}

template <class T>
T fetch_and_add(T *dst, T value, int target, int id)
{
    return comm::fetch_and_add(dst, value, target);
}
#else
template <class T>
void coll_allocate(size_t size, T ***ptrs, int *id)
{
    static size_t idx = 0;

    size_t n_procs = comm::get_n_procs();

    auto addr = (uint8_t *)0x40000000000 + 8192 * idx++;

    if (size < 8192)
        size = 8192;

    *id = comm::reg_coll_mmap(addr, sizeof(T) * size);

    *ptrs = new T *[n_procs];
    for (auto i = 0; i < n_procs; i++)
        (*ptrs)[i] = reinterpret_cast<T *>(addr);
}

void put(void *dst, void *src, size_t size, int target, int id)
{
    comm::put(dst, src, size, target);
}

void get(void *dst, void *src, size_t size, int target, int id)
{
    comm::get(dst, src, size, target);
}

template <class T>
T fetch_and_add(T *dst, T value, int target, int id)
{
    MADI_UNDEFINED;
}
#endif

static void real_main(int argc, char **argv)
{
    int me = comm::get_pid();
    int n_procs = comm::get_n_procs();

    if (argc < 2) {
        if (me == 0)
            fprintf(stderr, 
                    "Usage: %s [ put | get | acc ] data_size n_msgs ",
                    argv[0]);
        return;
    }
    int argidx = 1;
    const char *ctype = (argc >= argidx + 1) ? argv[argidx++] : "put";
    int data_size     = (argc >= argidx + 1) ? atoi(argv[argidx++]) : 8;
    int n_msgs        = (argc >= argidx + 1) ? atoi(argv[argidx++]) : 100;

    if (n_procs != 2) {
        if (me == 0)
            fprintf(stderr, "This program only supports two processes.\n");
        return;
    }

    int comm_type = 0;
    if (strcmp(ctype, "put") == 0)
        comm_type = 0;
    else if (strcmp(ctype, "get") == 0)
        comm_type = 1;
    else if (strcmp(ctype, "acc") == 0)
        comm_type = 2;

    if (me == 0) {
        printf("ctype = %s, data_size = %d, n_msgs = %d\n",
               ctype, data_size, n_msgs);
        fflush(stdout);
    }

    comm::barrier();

    int i;

    if (debug) {
        char debugname[1024];
        sprintf(debugname, "hoge.%02d.debug", me);

        g_fp = fopen(debugname, "w");
    }

    srand(rdtsc());

    int data_id, buf_id;
    uint8_t **data, **buf;
    coll_allocate<uint8_t>(data_size, &data, &data_id);
    coll_allocate<uint8_t>(data_size, &buf, &buf_id);

    size_t cachespill_size = 64 * 1024 * 1024;
    uint8_t *cachespill = new uint8_t[cachespill_size];

    memset(data[me], 0, data_size);
    memset(buf[me], 0, data_size);
    memset(cachespill, 0, cachespill_size);

    vector<timebuf> timebufs(n_msgs * 100);
    memset(timebufs.data(), 0, sizeof(timebuf) * timebufs.size());

    comm::barrier();

    int idx = 0;

    for (int size = 8; size <= data_size; size *= 2) {
        for (int i = 0; i < n_msgs; i++) {

//            memset(cachespill, (uint8_t)i, cachespill_size);

            comm::barrier();

            if (me == 0) {
                int target = 1;

                tsc_t t0 = rdtsc();

                if (comm_type == 0)
                    put(buf[target], data[me], size, target, buf_id);
                else if (comm_type == 1)
                    get(buf[me], data[target], size, target, data_id);
                else if (comm_type == 2)
                    fetch_and_add<uint64_t>((uint64_t *)buf[target], 100,
                                            target, buf_id);

                tsc_t t1 = rdtsc();

                comm::fence();

                tsc_t t2 = rdtsc();

                timebuf& tbuf = timebufs[idx++];
                tbuf.target = target;
                tbuf.size = size;
                tbuf.comm = t1 - t0;
                tbuf.fence = t2 - t1;
            }
        }
    }

    comm::barrier();

    if (me == 0) {
        long comm_sum = 0;
        long comm_max = 0;
        long comm_min = 1e10;
        for (i = 0; i < idx; i++) {
            const timebuf& tbuf = timebufs[i];
            fprintf(stdout, 
                    "pid = %9d, id = %9d, target = %9zu, "
                    "size = %9zu, comm = %9ld, fence = %9ld\n",
                    me, i, tbuf.target, tbuf.size, (long)tbuf.comm, (long)tbuf.fence);

            comm_sum += tbuf.comm;
            comm_max = (comm_max >= tbuf.comm) ? comm_max : tbuf.comm;
            comm_min = (comm_min <= tbuf.comm) ? comm_min : tbuf.comm;
        }
    }

#if 0
    long comm_avg = comm_sum / n_msgs;
    long comm_variance = 0;
    for (i = 0; i < n_msgs; i++) {
        const timebuf& tbuf = timebufs[i];

        comm_variance += (tbuf.comm - comm_avg) * (tbuf.comm - comm_avg);
    }
    comm_variance /= n_msgs;

    double comm_sdev = sqrt((double)comm_variance);

    if (comm_sum != 0) {
        printf("%05d: avg = %9ld, min = %9ld, max = %9ld, variance = %9ld, "
               "sdev = %9f\n",
               me, comm_avg, comm_min, comm_max, comm_variance, comm_sdev);
    }
#endif
}

int main(int argc, char **argv)
{
    comm::initialize(argc, argv);
    
    comm::start(real_main, argc, argv);

    comm::finalize();
    return 0;
}

