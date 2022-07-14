#include "mpi3/comm_base.h"
#include "mpi3/comm_memory.h"
#include "ampeer.h"
#include "options.h"
#include "madm_logger.h"

#include <cstring>
#include <mpi.h>

#define MADI_CB_DEBUG  0

#if MADI_CB_DEBUG
#define MADI_CB_DPUTS(s, ...)  MADI_DPUTS(s, ##__VA_ARGS__)
#else
#define MADI_CB_DPUTS(s, ...)
#endif

#ifndef MADI_MPI3_USE_CAS
#define MADI_MPI3_USE_CAS 1
#endif

namespace madi {
namespace comm {

    comm_base::comm_base(int& argc, char **& argv, amhandler_t handler)
        : cmr_(NULL)
        , comm_alc_(NULL)
        , value_buf_(NULL)
        , native_config_()
    {
        cmr_ = new comm_memory(native_config_);

        // initialize basic RDMA features (malloc/free/put/get)
        comm_alc_ = new allocator<comm_memory>(cmr_);

        value_buf_ = (long *)comm_alc_->allocate(sizeof(long), native_config_);
    }

    comm_base::~comm_base()
    {
        comm_alc_->deallocate((void *)value_buf_);
        delete comm_alc_;
        delete cmr_;
    }

    void ** comm_base::coll_malloc(size_t size, process_config& config)
    {
        int n_procs = config.get_n_procs();
        MPI_Comm comm = config.comm();
        comm_allocator *alc = comm_alc_;

        void **ptrs = new void *[n_procs];

        void *p = alc->allocate(size, config);

        MADI_ASSERT(p != NULL);

        MPI_Allgather(&p, sizeof(p), MPI_BYTE, 
                      ptrs, sizeof(p), MPI_BYTE,
                      comm);

        int me = config.get_pid();
        MADI_ASSERT(ptrs[me] != NULL);

        return ptrs;
    }

    void comm_base::coll_free(void **ptrs, process_config& config)
    {
        int me = config.get_pid();
        comm_allocator *alc = comm_alc_;

        alc->deallocate(ptrs[me]);

        delete [] ptrs;
    }

    void * comm_base::malloc(size_t size, process_config& config)
    {
        MPI_Comm comm = config.comm();

        // FIXME: non-collective implementation
        // currently, the comm_allocater::allocate function may call
        // collective function `extend_to'.
        MPI_Barrier(comm);

        comm_allocator *alc = comm_alc_;
        return alc->allocate(size, config);
    }

    void comm_base::free(void *p, process_config& config)
    {
        MPI_Comm comm = config.comm();

        // FIXME: non-collective implementation
        MPI_Barrier(comm);

        comm_allocator *alc = comm_alc_;
        alc->deallocate(p);
    }

    int comm_base::coll_mmap(uint8_t *addr, size_t size, process_config& config)
    {
        return cmr_->coll_mmap(addr, size, config);
    }

    void comm_base::coll_munmap(int memid, process_config& config)
    {
        cmr_->coll_munmap(memid, config);
    }

    void comm_base::put(void *dst, void *src, size_t size, int target,
                        process_config& config)
    {
        reg_put(-1, dst, src, size, target, config);
    }

    void comm_base::reg_put(int memid, void *dst, void *src, size_t size,
                            int target, process_config& config)
    {
        int pid = config.native_pid(target);
        int me = config.get_native_pid();
        raw_put<true>(memid, dst, src, size, pid, 0, me);
    }

    void comm_base::get(void *dst, void *src, size_t size, int target,
                        process_config& config)
    {
        reg_get(-1, dst, src, size, target, config);
    }

    void comm_base::reg_get(int memid, void *dst, void *src, size_t size,
                            int target, process_config& config)
    {
        int pid = config.native_pid(target);
        int me = config.get_native_pid();
        raw_get<true>(memid, dst, src, size, pid, 0, me);
    }

    void comm_base::put_nbi(void *dst, void *src, size_t size, int target,
                            process_config& config)
    {
        reg_put_nbi(-1, dst, src, size, target, config);
    }

    void comm_base::reg_put_nbi(int memid, void *dst, void *src, size_t size,
                                int target, process_config& config)
    {
        int pid = config.native_pid(target);
        int me = config.get_native_pid();
        raw_put<false>(memid, dst, src, size, pid, 0, me);
    }

    void comm_base::get_nbi(void *dst, void *src, size_t size, int target,
                            process_config& config)
    {
        reg_get_nbi(-1, dst, src, size, target, config);
    }

    void comm_base::reg_get_nbi(int memid, void *dst, void *src, size_t size,
                                int target, process_config& config)
    {
        int pid = config.native_pid(target);
        int me = config.get_native_pid();
        raw_get<false>(memid, dst, src, size, pid, 0, me);
    }

    template <bool BLOCKING>
    void comm_base::raw_put(int memid, void *dst, void *src, size_t size,
                            int target, int flags, int me)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::COMM_PUT>();

        if (target == me) {
            MADI_DPUTS3("memcpy(%p, %p, %zu)", dst, src, size);
            memcpy(dst, src, size);
            return;
        }

        MADI_ASSERT(0 <= target && target < native_config_.get_n_procs());

        comm_memory& cmr = *cmr_;

        // calculate local/remote buffer address
        MPI_Win win;
        size_t target_disp;
        cmr.translate(memid, dst, size, target, &target_disp, &win);

        // issue
        MPI_Put(src, size, MPI_BYTE, target, target_disp, size, MPI_BYTE, win);

        if (BLOCKING) {
            MPI_Win_flush(target, win);
        }

        logger::end_event<logger::kind::COMM_PUT>(bd, target);
    }

    template <bool BLOCKING>
    void comm_base::raw_get(int memid, void *dst, void *src, size_t size,
                            int target, int flags, int me)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::COMM_GET>();

        if (target == me) {
            MADI_DPUTS3("memcpy(%p, %p, %zu)", dst, src, size);
            memcpy(dst, src, size);
            return;
        }

        MADI_ASSERT(0 <= target && target < native_config_.get_n_procs());

        comm_memory& cmr = *cmr_;

        // calculate local/remote buffer address
        MPI_Win win;
        size_t target_disp;
        cmr.translate(memid, src, size, target, &target_disp, &win);

        // issue
        MPI_Get(dst, size, MPI_BYTE, target, target_disp, size, MPI_BYTE, win);

        if (BLOCKING) {
            MPI_Win_flush(target, win);
        }

        logger::end_event<logger::kind::COMM_GET>(bd, target);
    }

    int comm_base::poll(int *tag_out, int *pid_out, process_config& config)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::COMM_POLL>();

        // `MPI_Iprobe` is used to make progress on RMA operations, especially for MPICH.
        // Otherwise `MPI_Win_flush_all` gets stuck at the origin process because
        // the target process does not make progress on any RMA operations.
        // Setting `MPICH_ASYNC_PROGRESS=1` also resolves this issue without `MPI_Iprobe`,
        // but it will introduce additional overheads.
        //
        // References:
        // * https://lists.mpich.org/mailman/htdig/discuss/2014-September/001944.html
        // * https://community.intel.com/t5/Intel-oneAPI-HPC-Toolkit/MPI-polling-passive-rma-operations/td-p/1052066
        int flag;
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);

        sync();

        logger::end_event<logger::kind::COMM_POLL>(bd);
        return 0;
    }

    void comm_base::fence()
    {
        logger::begin_data bd = logger::begin_event<logger::kind::COMM_FENCE>();

        for (auto& win : cmr_->windows())
            if (win != MPI_WIN_NULL)
                MPI_Win_flush_all(win);

        logger::end_event<logger::kind::COMM_FENCE>(bd);
    }

    void comm_base::sync()
    {
        for (auto& win : cmr_->windows())
            if (win != MPI_WIN_NULL)
                MPI_Win_sync(win);
    }

    void comm_base::native_barrier(process_config& config)
    {
        MPI_Comm comm = config.comm();
        MPI_Barrier(comm);
    }

    template <class T> inline MPI_Datatype mpi_type();
    template <> inline MPI_Datatype mpi_type<int>() { return MPI_INT; }
    template <> inline MPI_Datatype mpi_type<long>() { return MPI_LONG; }
    template <> inline MPI_Datatype mpi_type<unsigned long>() { return MPI_UNSIGNED_LONG; }


    template <class T>
    T comm_base::fetch_and_add(T *dst, T value, int target,
                               process_config& config)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::COMM_FETCH_AND_ADD>();

        // calculate local/remote buffer address
        MPI_Win win;
        size_t target_disp;
        cmr_->translate(-1, dst, sizeof(T), target, &target_disp, &win);

        MPI_Datatype type = mpi_type<T>();

        // issue
        T result;
        MPI_Fetch_and_op(&value, &result, type, target, target_disp,
                         MPI_SUM, win);
        MPI_Win_flush(target, win);

        logger::end_event<logger::kind::COMM_FETCH_AND_ADD>(bd, target);

        return result;
    }

    template int comm_base::fetch_and_add<int>(int *, int, int,
                                               process_config&);
    template long comm_base::fetch_and_add<long>(long *, long, int,
                                                 process_config&);
    template unsigned long comm_base::fetch_and_add<unsigned long>(unsigned long *, unsigned long, int,
                                                                   process_config&);

    void comm_base::lock_init(lock_t* lp, process_config& config)
    {
        *lp = 0;
    }

    bool comm_base::trylock(lock_t* lp, int target,
                             process_config& config)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::COMM_TRYLOCK>();

        MPI_Win win;
        size_t target_disp;
        cmr_->translate(-1, lp, sizeof(lock_t), target, &target_disp, &win);

        lock_t result;
#if MADI_MPI3_USE_CAS
        constexpr lock_t zero = 0;
        constexpr lock_t one = 1;
        MPI_Compare_and_swap(&one, &zero, &result, MPI_LOCK_T, target, target_disp, win);
#else
        constexpr lock_t inc = 1;
        MPI_Fetch_and_op(&inc, &result, MPI_LOCK_T, target, target_disp, MPI_SUM, win);
#endif
        MPI_Win_flush(target, win);

        logger::end_event<logger::kind::COMM_TRYLOCK>(bd, target);

        return result == 0;
    }

    void comm_base::lock(lock_t* lp, int target,
                         process_config& config)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::COMM_LOCK>();

        while (!trylock(lp, target, config)) {
            int tag, pid;
            poll(&tag, &pid, config);
        };

        logger::end_event<logger::kind::COMM_LOCK>(bd, target);
    }

    void comm_base::unlock(lock_t* lp, int target,
                           process_config& config)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::COMM_UNLOCK>();

        MPI_Win win;
        size_t target_disp;
        cmr_->translate(-1, lp, sizeof(lock_t), target, &target_disp, &win);

        constexpr lock_t zero = 0;
        lock_t result;
        MPI_Fetch_and_op(&zero, &result, MPI_LOCK_T, target, target_disp, MPI_REPLACE, win);
        MPI_Win_flush(target, win);

        logger::end_event<logger::kind::COMM_UNLOCK>(bd, target);
    }
}
}
