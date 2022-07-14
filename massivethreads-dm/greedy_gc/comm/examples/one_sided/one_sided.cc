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

static void warmup(int me, int n_procs, uint8_t **data, uint8_t **buf, 
                   size_t data_size, int comm_type, int n_warmups,
                   int data_id, int buf_id)
{
    if (n_warmups <= 0)
        return;

    for (auto target = 0; target < n_procs; target++) {
        for (auto i = 0; i < n_warmups; i++) {
            if (comm_type == 0)
                put(buf[target], data[me], data_size, target, buf_id);
            else if (comm_type == 1)
                get(buf[me], data[target], data_size, target, data_id);
            else if (comm_type == 2) {
                fetch_and_add<uint64_t>((uint64_t *)buf[target], 100, target, buf_id);
            }

            comm::fence();
        }
    }
}

//extern bool ampeer_prof_enabled;

static void real_main(int argc, char **argv)
{
//    ampeer_prof_enabled = false;

    int me = comm::get_pid();
    int n_procs = comm::get_n_procs();

    if (argc < 2) {
        if (me == 0)
            fprintf(stderr, 
                    "Usage: %s [ put | get | acc ] "
                    "n_msgs sender_mod "
                    "do_warmup\n", argv[0]);
        return;
    }
    int argidx = 1;
    const char *ctype = (argc >= argidx + 1) ? argv[argidx++] : "put";
    int n_msgs        = (argc >= argidx + 1) ? atoi(argv[argidx++]) : 100;
    int sender_mod    = (argc >= argidx + 1) ? atoi(argv[argidx++]) : n_procs;
    int n_warmups     = (argc >= argidx + 1) ? atoi(argv[argidx++]) : 0;

    int comm_type = 0;
    if (strcmp(ctype, "put") == 0)
        comm_type = 0;
    else if (strcmp(ctype, "get") == 0)
        comm_type = 1;
    else if (strcmp(ctype, "acc") == 0)
        comm_type = 2;

    if (me == 0) {
        printf("ctype = %s, n_msgs = %d, sender_mod = %d, n_warmups = %d\n",
               ctype, n_msgs, sender_mod, n_warmups);
    }

    comm::barrier();

    int i;

    if (debug) {
        char debugname[1024];
        sprintf(debugname, "hoge.%02d.debug", me);

        g_fp = fopen(debugname, "w");
    }

    srand(rdtsc());

    size_t data_size = 8;

    int data_id, buf_id;
    uint8_t **data, **buf;
    coll_allocate<uint8_t>(data_size, &data, &data_id);
    coll_allocate<uint8_t>(data_size, &buf, &buf_id);

    memset(data[me], 0, data_size);
    memset(buf[me], 0, data_size);

    warmup(me, n_procs, data, buf, data_size, comm_type, n_warmups,
           data_id, buf_id);

    vector<timebuf> timebufs(n_msgs);
    memset(timebufs.data(), 0, sizeof(timebuf) * n_msgs);

    comm::barrier();

    MADI_DPUTSB2("OPS START");

    if (me % sender_mod == 0) {
        int idx = 0;
        for (i = 0; i < n_msgs; i++) {
            int target;
            do {
                target = random_int(n_procs);
            } while (n_procs != 1 && target == me);

            tsc_t t0 = rdtsc();

            if (comm_type == 0)
                put(buf[target], data[me], data_size, target, buf_id);
            else if (comm_type == 1)
                get(buf[me], data[target], data_size, target, data_id);
            else if (comm_type == 2)
                fetch_and_add<uint64_t>((uint64_t *)buf[target], 100,
                                        target, buf_id);

            tsc_t t1 = rdtsc();
            
            comm::fence();

            tsc_t t2 = rdtsc();

            timebuf& tbuf = timebufs[idx++];
            tbuf.target = target;
            tbuf.comm = t1 - t0;
            tbuf.fence = t2 - t1;
        }
    }

    MADI_DPUTSB2("OPS LOCALY DONE");

    comm::barrier();

    MADI_DPUTSB2("OPS GLOBALLY DONE");

    char filename[1024];
    sprintf(filename, "one_sided.%05d.out", me);

    FILE *fp = fopen(filename, "w");

    long comm_sum = 0;
    long comm_max = 0;
    long comm_min = 1e10;
    for (i = 0; i < n_msgs; i++) {
        const timebuf& tbuf = timebufs[i];
        fprintf(fp, 
                "pid = %9d, id = %9d, target = %9zu, "
                "comm = %9ld, fence = %9ld\n",
                me, i, tbuf.target, (long)tbuf.comm, (long)tbuf.fence);

        comm_sum += tbuf.comm;
        comm_max = (comm_max >= tbuf.comm) ? comm_max : tbuf.comm;
        comm_min = (comm_min <= tbuf.comm) ? comm_min : tbuf.comm;
    }

    fclose(fp);


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
}

int main(int argc, char **argv)
{
    comm::initialize(argc, argv);
    
//    ampeer_prof_enabled = true;
    comm::start(real_main, argc, argv);
//    ampeer_prof_enabled = false;

    comm::finalize();
    return 0;
}

