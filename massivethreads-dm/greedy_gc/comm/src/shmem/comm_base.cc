#include <cstring>

#include "shmem/comm_base.h"
#include "options.h"

namespace madi {
namespace comm {

    comm_base::comm_base(int& argc, char **& argv, amhandler_t handler)
        : native_config_()
    {
        cm_ = std::make_unique<comm_memory>(native_config_);
        comm_alc_ = std::make_unique<cm_allocator>(cm_.get());
    }

    void ** comm_base::coll_malloc(size_t size, process_config& config)
    {
        int n_procs = config.get_n_procs();
        MPI_Comm comm = config.comm();

        void **ptrs = new void *[n_procs];

        void *p = comm_alc_->allocate(size, config);

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

        comm_alc_->deallocate(ptrs[me]);

        delete [] ptrs;
    }

    void * comm_base::malloc(size_t size, process_config& config)
    {
        MPI_Comm comm = config.comm();

        // FIXME: non-collective implementation
        // currently, the comm_allocater::allocate function may call
        // collective function `extend_to'.
        MPI_Barrier(comm);

        return comm_alc_->allocate(size, config);
    }

    void comm_base::free(void *p, process_config& config)
    {
        MPI_Comm comm = config.comm();

        // FIXME: non-collective implementation
        MPI_Barrier(comm);

        comm_alc_->deallocate(p);
    }

    int comm_base::coll_mmap(uint8_t *addr, size_t size, process_config& config)
    {
        return cm_->coll_mmap(addr, size, config);
    }

    void comm_base::coll_munmap(int memid, process_config& config)
    {
        cm_->coll_munmap(memid, config);
    }

    void comm_base::put_nbi(void *dst, void *src, size_t size, int target,
                            process_config& config)
    {
        do_put(comm_memory::MEMID_DEFAULT, dst, src, size, target, config);
    }

    void comm_base::reg_put_nbi(int memid, void *dst, void *src, size_t size,
                                int target, process_config& config)
    {
        do_put(memid, dst, src, size, target, config);
    }

    void comm_base::get_nbi(void *dst, void *src, size_t size, int target,
                            process_config& config)
    {
        do_get(comm_memory::MEMID_DEFAULT, dst, src, size, target, config);
    }

    void comm_base::reg_get_nbi(int memid, void *dst, void *src, size_t size,
                                int target, process_config& config)
    {
        do_get(memid, dst, src, size, target, config);
    }

    void comm_base::do_put(int memid, void *dst, void *src, size_t size,
                           int target, process_config& config)
    {
        int me = config.get_native_pid();

        void *remote_dst = cm_->translate(memid, dst, size, target);
        void *local_src  = cm_->translate(memid, src, size, me);

        memcpy(remote_dst, local_src, size);
    }

    void comm_base::do_get(int memid, void *dst, void *src, size_t size,
                           int target, process_config& config)
    {
        int me = config.get_native_pid();

        void *local_dst  = cm_->translate(memid, dst, size, me);
        void *remote_src = cm_->translate(memid, src, size, target);

        memcpy(local_dst, remote_src, size);
    }

    int comm_base::poll(int *tag_out, int *pid_out, process_config& config)
    {
        // do nothing
        return 0;
    }

    void comm_base::fence()
    {
        threadsafe::rwbarrier();
    }

    void comm_base::native_barrier(process_config& config)
    {
        MPI_Comm comm = config.comm();
        MPI_Barrier(comm);
    }
}
}
