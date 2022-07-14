#include "uth_comm.h"
#include "uth_comm-inl.h"

#include <madm_comm.h>

namespace madi {

    using namespace madi::comm;

    static bool amhandle(int tag, int pid, void *data, size_t size,
                         aminfo *info)
    {
        return false;
    }

    bool uth_comm::initialize(int& argc, char **&argv)
    {
        comm::initialize_with_amhandler(argc, argv, amhandle);

        buffer_size_ = 8192;

        buffer_ = comm::rma_malloc<uint8_t>(buffer_size_);

        initialized_ = true;

        return true;
    }

    void uth_comm::finalize()
    {
        comm::rma_free(buffer_);
        comm::finalize();

        buffer_size_ = 0;
        buffer_ = NULL;
    }

    void uth_comm::exit(int exitcode)
    {
        comm::exit(exitcode);
    }

    void ** uth_comm::malloc_shared(size_t size)
    {
        void **ptrs = (void **)comm::coll_rma_malloc<uint8_t>(size);

        if (ptrs == NULL) {
            MADI_SPMD_DIE("cannot allocate RMA memory (size = %zu)",
                          size);
        }

        return ptrs;
    }

    void uth_comm::free_shared(void **p)
    {
        comm::coll_rma_free((uint8_t **)p);
    }

    void * uth_comm::malloc_shared_local(size_t size)
    {
        return (void *)comm::rma_malloc<uint8_t>(size);
    }

    void uth_comm::free_shared_local(void *p)
    {
        comm::rma_free((uint8_t *)p);
    }

    void uth_comm::put(void *dst, void *src, size_t size, uth_pid_t target)
    {
        comm::put(dst, src, size, target);
    }

    void uth_comm::get(void *dst, void *src, size_t size, uth_pid_t target)
    {
        comm::get(dst, src, size, target);
    }

    void uth_comm::put_nbi(void *dst, void *src, size_t size, uth_pid_t target)
    {
        comm::put_nbi(dst, src, size, target);
    }

    void uth_comm::get_nbi(void *dst, void *src, size_t size, uth_pid_t target)
    {
        comm::get_nbi(dst, src, size, target);
    }

    void uth_comm::put_value(int *dst, int value, uth_pid_t target)
    {
        comm::put_value<int>(dst, value, target);
    }

    void uth_comm::put_value(long *dst, long value, uth_pid_t target)
    {
        comm::put_value<long>(dst, value, target);
    }

    void uth_comm::put_value(uint64_t *dst, uint64_t value, uth_pid_t target)
    {
        comm::put_value<uint64_t>(dst, value, target);
    }

    int uth_comm::get_value(int *src, uth_pid_t target)
    {
        return comm::get_value<int>(src, target);
    }

    long uth_comm::get_value(long *src, uth_pid_t target)
    {
        return comm::get_value<long>(src, target);
    }

    uint64_t uth_comm::get_value(uint64_t *src, uth_pid_t target)
    {
        return comm::get_value<uint64_t>(src, target);
    }

    void uth_comm::swap(int *dst, int *src, uth_pid_t target)
    {
        MADI_UNDEFINED;
    }

    void uth_comm::lock_init(lock_t* lp)
    {
        return comm::lock_init(lp);
    }

    bool uth_comm::trylock(lock_t* lp, uth_pid_t target)
    {
        return comm::trylock(lp, target);
    }

    void uth_comm::lock(lock_t* lp, uth_pid_t target)
    {
        comm::lock(lp, target);
    }

    void uth_comm::unlock(lock_t* lp, uth_pid_t target)
    {
        comm::unlock(lp, target);
    }

    void ** uth_comm::reg_mmap_shared(void *addr, size_t size)
    {
        // NOTE: this function can be called at a time
        MADI_CHECK(rdma_id_ == -1);

        size_t n_procs = get_n_procs();

        void **ptrs = new void *[n_procs];

        for (size_t i = 0; i < n_procs; i++)
            ptrs[i] = addr;

        rdma_id_ = comm::reg_coll_mmap(addr, size);

        if (rdma_id_ != -1)
            return ptrs;

        // mcomm library does not support mmap with memory registration
        // in GASNet. Instead, use malloc_shared which is implemented as 
        // all allocated addresses are same (if previous allocations are all 
        // done by specifying the same size arguments).

        delete ptrs;

        ptrs = malloc_shared(size);

        // check whether all addresses are same or not
        bool ok = true;
        uint8_t *true_addr = reinterpret_cast<uint8_t *>(ptrs[0]);
        for (size_t i = 1; i < n_procs; i++) {
            if (ptrs[i] != true_addr) {
                ok = false;
                break;
            }
        }

        rdma_id_ = -2;

        if (ok) {
            return ptrs;
        } else {
            free_shared(ptrs);
            return NULL;
        }
    }

    void uth_comm::reg_munmap_shared(void **ptrs, void *addr, size_t size)
    {
        MADI_CHECK(rdma_id_ != -1);

        if (rdma_id_ != -2) {
            comm::reg_coll_munmap(rdma_id_);
            delete ptrs;
        } else {
            free_shared(ptrs);
        }

        rdma_id_ = -1;
    }

    void uth_comm::reg_put(void *dst, void *src, size_t size,
                           uth_pid_t target)
    {
        MADI_ASSERT(rdma_id_ != -1);

        comm::reg_put(rdma_id_, dst, src, size, target);
    }

    void uth_comm::reg_get(void *dst, void *src, size_t size,
                           uth_pid_t target)
    {
        MADI_ASSERT(rdma_id_ != -1);

        comm::reg_get(rdma_id_, dst, src, size, target);
    }

    void uth_comm::barrier()
    {
        comm::barrier();
    }

    bool uth_comm::barrier_try()
    {
        return comm::barrier_try();
    }

    void uth_comm::poll()
    {
        comm::poll();
    }

    // active messages (experimental)

    void uth_comm::amrequest(int tag, void *p, size_t size, int target)
    {
        comm::amrequest(tag, p, size, target);
    }

    void uth_comm::amreply(int tag, void *p, size_t size, aminfo *info)
    {
        comm::amreply(tag, p, size, info);
    }
}

