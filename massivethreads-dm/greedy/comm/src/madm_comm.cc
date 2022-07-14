#include "madm_comm.h"
#include "madm_misc.h"
#include "options.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace madi {
namespace comm {

    globals g;

    bool initialize(int& argc, char **& argv)
    {
        return initialize_with_amhandler(argc, argv, nullptr);
    }

    bool initialize_with_amhandler(int& argc, char **& argv,
                                   amhandler_t handler)
    {
        options_initialize();

        MADI_DPUTS2("madm::comm start initialization");

        int init;
        MPI_Initialized(&init);

        if (!init)
            MPI_Init(&argc, &argv);

        g.debug_out = stderr;
        MPI_Comm_rank(MPI_COMM_WORLD, &g.debug_pid);
        int nproc;
        MPI_Comm_size(MPI_COMM_WORLD, &nproc);

        g.comm = new comm_system(argc, argv, handler);

        global_clock::init();

        logger::init(g.debug_pid, nproc);
        logger::checkpoint<logger::kind::INIT>();

        g.initialized = true;

        g.comm->barrier();

        return true;
    }

    void finalize()
    {
        g.comm->barrier();

        delete g.comm;
        g.comm = NULL;

        MPI_Finalize();

        options_finalize();
    }

    void exit(int exitcode)
    {
        std::exit(exitcode);
    }

    void abort(int exitcode)
    {
        MPI_Abort(MPI_COMM_WORLD, exitcode);
        std::exit(exitcode); // for suppressing warning
    }

    void poll()
    {
        int tag, pid;

        int r = g.comm->poll(&tag, &pid);

        // r == 0 as long as user code does not call FJMPI_Rdma_*.
        // because FJMPI_Rdma_poll_cq does not support modular use of
        // FJMPI_Rdma_poll_cq, mcomm may steal completion queue entry
        // arisen from user code.
        // FIXME: provide API for user to poll completion queue safely.
        if (r != 0) {
            MADI_DIE("unknown entry of completion queue "
                     "(r = %d, tag = %d, pid = %d)",
                     r, tag, pid);
        }
    }

    void fence()
    {
        g.comm->fence();
    }

    void put_nbi(void *dst, void *src, size_t size, pid_t target)
    {
        MADI_ASSERT(0 <= target && target < get_n_procs());

        g.comm->put_nbi(dst, src, size, target);
    }

    void put(void *dst, void *src, size_t size, pid_t target)
    {
        MADI_ASSERT(0 <= target && target < get_n_procs());

        g.comm->put(dst, src, size, target);
    }

    void get_nbi(void *dst, void *src, size_t size, pid_t target)
    {
        MADI_ASSERT(0 <= target && target < get_n_procs());

        g.comm->get_nbi(dst, src, size, target);
    }

    void get(void *dst, void *src, size_t size, pid_t target)
    {
        MADI_ASSERT(0 <= target && target < get_n_procs());

        g.comm->get(dst, src, size, target);
    }

    int reg_coll_mmap(void *addr, size_t size)
    {
        return g.comm->coll_mmap((uint8_t *)addr, size);
    }

    void reg_coll_munmap(int id)
    {
        g.comm->coll_munmap(id);
    }

    void reg_put(int id, void *dst, void *src, size_t size, pid_t target)
    {
        MADI_ASSERT(0 <= target && target < get_n_procs());

        g.comm->reg_put(id, dst, src, size, target);
    }

    void reg_get(int id, void *dst, void *src, size_t size, pid_t target)
    {
        MADI_ASSERT(0 <= target && target < get_n_procs());

        g.comm->reg_get(id, dst, src, size, target);
    }

    void barrier()
    {
        g.comm->barrier();
    }

    bool barrier_try()
    {
        return g.comm->barrier_try();
    }

    template <class T>
    void broadcast(T* buf, size_t size, pid_t root)
    {
        MADI_ASSERT(0 <= root && root < get_n_procs());

        g.comm->broadcast(buf, size, root);
    }

    template void broadcast(int*, size_t, pid_t);
    template void broadcast(unsigned int*, size_t, pid_t);
    template void broadcast(long*, size_t, pid_t);
    template void broadcast(unsigned long*, size_t, pid_t);

    template <class T>
    void reduce(T *dst, const T src[], size_t size, pid_t root,
                reduce_op op)
    {
        g.comm->reduce(dst, src, size, root, op);
    }

    template void reduce(int *, const int *, size_t, pid_t, reduce_op);
    template void reduce(unsigned int *, const unsigned int *, size_t, pid_t,
                         reduce_op);
    template void reduce(long *, const long *, size_t, pid_t, reduce_op);
    template void reduce(unsigned long *, const unsigned long *, size_t,
                         pid_t, reduce_op);

    void native_barrier()
    {
        g.comm->native_barrier();
    }

    void amrequest(int tag, void *p, size_t size, int target)
    {
        MADI_ASSERT(0 <= target && (size_t)target < get_n_procs());

        g.comm->request(tag, p, size, target);
    }

    void amreply(int tag, void *p, size_t size, aminfo *info)
    {
        g.comm->reply(tag, p, size, info);
    }

    size_t get_server_mod()
    {
        return options.server_mod;
    }

    void print_options(FILE *f)
    {
        madi::comm::options_print(f);
    }
}
}

extern "C" {
    
    int madi_initialized()
    {
        return (int)madi::comm::initialized();
    }

    size_t madi_get_debug_pid()
    {
        return madi::comm::g.debug_pid;
        //return madi::comm::g.comm_native->get_pid();
    }

    void madi_exit(int exitcode)
    {
        madi::comm::exit(exitcode);
    }

    int madi_dprint_raw(const char *format, ...)
    {
        va_list arg;

        FILE *out = madi::comm::g.debug_out;

        va_start(arg, format);
        int r = vfprintf(out, format, arg);
        va_end(arg);

        fflush(out);

        return r;
    }

}
