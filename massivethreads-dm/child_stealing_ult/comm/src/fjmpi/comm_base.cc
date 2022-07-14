#include "fjmpi/comm_base.h"
#include "fjmpi/comm_memory.h"
#include "ampeer.h"
#include "options.h"

#include <mpi.h>
#include <mpi-ext.h>

#define MADI_CB_DEBUG  0

#if MADI_CB_DEBUG
#define MADI_CB_DPUTS(s, ...)  MADI_DPUTS(s, ##__VA_ARGS__)
#else
#define MADI_CB_DPUTS(s, ...)
#endif

namespace madi {
namespace comm {

    constexpr int FJMPI_TAG = 1;
    constexpr int FJMPI_AMTAG_BASE = 2;

    comm_core::comm_core(int fjmpi_tag)
        : tag_(fjmpi_tag)
        , rdma_handle_(NULL)
        , cmr_(NULL)
        , comm_alc_(NULL)
        , value_buf_(NULL)
        , native_config_()
    {
        rdma_handle_ = new rdma_handle();
        cmr_ = new comm_memory(native_config_);

        // initialize basic RDMA features (malloc/free/put/get)
        comm_alc_ = new allocator<comm_memory>(cmr_);

        value_buf_ = (long *)comm_alc_->allocate(sizeof(long), native_config_);
    }

    comm_core::~comm_core()
    {
        comm_alc_->deallocate((void *)value_buf_);
        delete comm_alc_;
        delete cmr_;
        delete rdma_handle_;
    }

    void ** comm_core::coll_malloc(size_t size, process_config& config)
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

    void comm_core::coll_free(void **ptrs, process_config& config)
    {
        int me = config.get_pid();
        comm_allocator *alc = comm_alc_;

        alc->deallocate(ptrs[me]);

        delete [] ptrs;
    }

    void * comm_core::malloc(size_t size, process_config& config)
    {
        MPI_Comm comm = config.comm();

        // FIXME: non-collective implementation
        // currently, the comm_allocater::allocate function may call
        // collective function `extend_to'.
        MPI_Barrier(comm);

        comm_allocator *alc = comm_alc_;
        return alc->allocate(size, config);
    }

    void comm_core::free(void *p, process_config& config)
    {
        MPI_Comm comm = config.comm();

        // FIXME: non-collective implementation
        MPI_Barrier(comm);

        comm_allocator *alc = comm_alc_;
        alc->deallocate(p);
    }

    int comm_core::coll_mmap(uint8_t *addr, size_t size, process_config& config)
    {
        return cmr_->coll_mmap(addr, size, config);
    }

    void comm_core::coll_munmap(int memid, process_config& config)
    {
        cmr_->coll_munmap(memid, config);
    }

    void comm_core::put_nbi(void *dst, void *src, size_t size, int target,
                            process_config& config)
    {
        reg_put_nbi(-1, dst, src, size, target, config);
    }

    void comm_core::reg_put_nbi(int memid, void *dst, void *src, size_t size,
                                int target, process_config& config)
    {
        int pid = config.native_pid(target);
        int me = config.get_native_pid();
        raw_put__(memid, dst, src, size, pid, tag_, 0, me);
    }

    void comm_core::get_nbi(void *dst, void *src, size_t size, int target,
                            process_config& config)
    {
        reg_get_nbi(-1, dst, src, size, target, config);
    }

    void comm_core::reg_get_nbi(int memid, void *dst, void *src, size_t size,
                                int target, process_config& config)
    {
        int pid = config.native_pid(target);
        int me = config.get_native_pid();
        raw_get(memid, dst, src, size, pid, tag_, 0, me);
    }

    void comm_core::raw_put(int tag, void *dst, void *src, size_t size,
                            int target, int me)
    {
        raw_put__(-1, dst, src, size, target, tag, 0, me);
        /* no handle */
    }

    void comm_core::raw_put_ordered(int tag, void *dst, void *src, size_t size,
                                    int target, int me)
    {
        raw_put__(-1, dst, src, size, target, tag, FJMPI_RDMA_STRONG_ORDER,
                me);
        /* no handle */
    }

    void comm_core::raw_put_with_notice(int tag, void *dst, void *src,
                                        size_t size, int target, int me)
    {
        raw_put__(-1, dst, src, size, target, tag, FJMPI_RDMA_REMOTE_NOTICE,
                  me);
        /* no handle */
    }

    void comm_core::raw_put__(int memid, void *dst, void *src, size_t size,
                              int target, int tag, int flags, int me)
    {
        if (target == me) {
            MADI_DPUTS3("memcpy(%p, %p, %zu)", dst, src, size);
            memcpy(dst, src, size);
            return;
        }

        rdma_handle& handle = *rdma_handle_;
        comm_memory& cmr = *cmr_;

        // calculate local/remote buffer address
        uint64_t raddr, laddr;
        if (memid == -1) {
            raddr = cmr.translate(dst, size, target);
            laddr = cmr.translate(src, size, me);
        } else {
            raddr = cmr.translate(dst, size, target, memid);
            laddr = cmr.translate(src, size, me, memid);
        }

        MADI_ASSERT(0 <= target && target < native_config_.get_n_procs());

        MADI_CB_DPUTS("PUT(target=%d, tag=%d, raddr=0x%x, laddr=0x%x, %zu)",
                   target, tag, raddr, laddr, size);

        // issue
        FJMPI_Rdma_put(target, tag, raddr, laddr, size, flags);

        if (tag == tag_)
            handle.incr();
    }

    void comm_core::raw_get(int memid, void *dst, void *src, size_t size,
                 int target, int tag, int flags, int me)
    {
        if (target == me) {
            MADI_DPUTS3("memcpy(%p, %p, %zu)", dst, src, size);
            memcpy(dst, src, size);
            return;
        }

        rdma_handle& handle = *rdma_handle_;
        comm_memory& cmr = *cmr_;

        // calculate local/remote buffer address
        uint64_t raddr, laddr;
        if (memid == -1) {
            raddr = cmr.translate(src, size, target);
            laddr = cmr.translate(dst, size, me);
        } else {
            raddr = cmr.translate(src, size, target, memid);
            laddr = cmr.translate(dst, size, me, memid);
        }

        MADI_ASSERT(0 <= target && target < native_config_.get_n_procs());

        // issue
        FJMPI_Rdma_get(target, tag, raddr, laddr, size, flags);

        if (tag == tag_)
            handle.incr();
    }

    int comm_core::ccpoll(int *tag_out, int *pid_out, process_config& config)
    {
        FJMPI_Rdma_cq cq;
        int r = FJMPI_Rdma_poll_cq(FJMPI_RDMA_NIC0, &cq);

        if (r == 0)
            return 0;

        if (MADI_CB_DEBUG) {
            const char *r_str = "UNKNOWN";
            if (r == FJMPI_RDMA_NOTICE)
                r_str = "FJMPI_RDMA_NOTICE";
            else if (r == FJMPI_RDMA_HALFWAY_NOTICE)
                r_str = "FJMPI_RDMA_HALFWAY_NOTICE";

            MADI_CB_DPUTS("FJMPI_Rdma_poll_cq: r = %s, tag = %d, pid = %d",
                          r_str, cq.tag, cq.pid);
        }

        int tag = tag_;
        rdma_handle& handle = *rdma_handle_;

        if (cq.tag == tag) {
            if (r == FJMPI_RDMA_NOTICE)
                handle.decr();
            else
                MADI_NOT_REACHED;

            MADI_CB_DPUTS("ACK local completion");
            return 0;
        }

        *tag_out = cq.tag;
        *pid_out = cq.pid;
        return r;
    }

    void comm_core::ccfence()
    {
        rdma_handle& handle = *rdma_handle_;

        while (!handle.done())
            madi::comm::poll();
    }

    void comm_core::native_barrier(process_config& config)
    {
        MPI_Comm comm = config.comm();
        MPI_Barrier(comm);
    }

    comm_base::comm_base(int& argc, char **& argv, amhandler_t handler)
        : comm_core(FJMPI_TAG)
        , ampeer<comm_base>(*this, handler, options.server_mod,
                            options.n_max_sends, FJMPI_AMTAG_BASE)
    {
    }

    void comm_base::fence()
    {
        ccfence();
        amfence();
    }

    int comm_base::poll(int *tag_out, int *pid_out, process_config& config)
    {
        ampoll(config);

        int tag, pid;
        int r = ccpoll(&tag, &pid, config);

        if (r == 0)
            return r;

        bool processed = amhandle(r, tag, pid, config);

        if (processed) {
            MADI_CB_DPUTS("DONE handle an active message");
            return 0;
        }

        *tag_out = tag;
        *pid_out = pid;
        return r;
    }

}
}

#include "ampeer-inl.h"

namespace madi {
namespace comm {

    // template instantiation for comm_base class
    template class ampeer<comm_base>;
    //template int ampeer<comm_base>::
    //  fetch_and_add(int *dst, int value, int target, process_config& config);
    //template long ampeer<comm_base>::
    //  fetch_and_add(long *dst, long value, int target, process_config& config);
    template uint64_t ampeer<comm_base>::
        fetch_and_add(uint64_t *dst, uint64_t value, int target,
                      process_config& config);


}
}

