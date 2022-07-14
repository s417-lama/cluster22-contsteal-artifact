#include "ibv/comm_base.h"
#include "ibv/comm_memory.h"
#include "sys.h"
#include "threadsafe.h"
#include "options.h"

#include <mpi.h>


#define MADI_CB_DEBUG  0

#if MADI_CB_DEBUG
#define MADI_CB_DPUTS(s, ...)  MADI_DPUTS(s, ##__VA_ARGS__)
#else
#define MADI_CB_DPUTS(s, ...)
#endif

namespace madi {
namespace comm {

    uint8_t * mmap_rdma_segment(size_t size)
    {
        auto addr = reinterpret_cast<void *>(0x30000000000);

        auto prot = PROT_READ | PROT_WRITE;
        auto flags = MAP_ANONYMOUS | MAP_PRIVATE;
        auto p = mmap(addr, size, prot, flags, -1, 0);

        if (p == MAP_FAILED)
            MADI_SPMD_PERR_DIE("mmap");

        if (p != addr)
            MADI_SPMD_DIE("cannot allocate fixed-address memory.");

        return reinterpret_cast<uint8_t *>(p);
    }

    void munmap_rdma_segment(uint8_t *addr, size_t size)
    {
        munmap(reinterpret_cast<void *>(addr), size);
    }

    /*
     * current implementation is EXPERIMENTAL. it only supports:
     * - the size of RDMA-capable memory is up to Infiniband limit
     *   (<2GB on TSUBAME2.5)
     * - the data size of RDMA READ/WRITE is up to Infiniband limit
     */
    comm_base::comm_base(int& argc, char **& argv)
        : native_config_()
        , buffer_size_(4 * 1024 * 1024)
        , buffer_(mmap_rdma_segment(buffer_size_))
        , env_()
        , ep_(env_, reinterpret_cast<void *>(buffer_), buffer_size_)
        , cm_(buffer_, buffer_size_, native_config_)
        , comm_alc_(&cm_)
        , value_buf_(reinterpret_cast<uint64_t *>(
                         this->malloc(sizeof(uint64_t), native_config_)))
    {
    }

    comm_base::~comm_base()
    {
        munmap_rdma_segment(buffer_, buffer_size_);
    }

    void ** comm_base::coll_malloc(size_t size, process_config& config)
    {
        int n_procs = config.get_n_procs();
        MPI_Comm comm = config.comm();
        comm_allocator& alc = comm_alc_;

        void **ptrs = new void *[n_procs];

        void *p = alc.allocate(size, config);

        MADI_ASSERT(p != NULL);

        if (p == NULL) {
            MADI_DPUTS("coll_malloc cannot allocate memory.");
            return NULL;
        }

        MPI_Allgather(&p, sizeof(p), MPI_BYTE, 
                      ptrs, sizeof(p), MPI_BYTE,
                      comm);

        int me = config.get_pid();
        MADI_ASSERT(ptrs[me] != NULL);

        return ptrs;
    }

    void comm_base::coll_free(void **ptrs, process_config& config)
    {
        if (ptrs == NULL)
            return;

        int me = config.get_pid();
        comm_allocator& alc = comm_alc_;

        alc.deallocate(ptrs[me]);

        delete [] ptrs;
    }

    void * comm_base::malloc(size_t size, process_config& config)
    {
        MPI_Comm comm = config.comm();

        // FIXME: non-collective implementation
        // currently, the comm_allocater::allocate function may call
        // collective function `extend_to'.
        MPI_Barrier(comm);

        comm_allocator& alc = comm_alc_;
        return alc.allocate(size, config);
    }

    void comm_base::free(void *p, process_config& config)
    {
        MPI_Comm comm = config.comm();

        // FIXME: non-collective implementation
        MPI_Barrier(comm);

        comm_allocator& alc = comm_alc_;
        alc.deallocate(p);
    }

    int comm_base::coll_mmap(uint8_t *addr, size_t size, process_config& config)
    {
        return -1;
    }

    void comm_base::coll_munmap(int memid, process_config& config)
    {
        MADI_UNDEFINED;
    }

    void comm_base::put_nbi(void *dst, void *src, size_t size, int target,
                            process_config& config)
    {
        ep_.rdma_put(dst, src, size, target);
    }

    void comm_base::reg_put_nbi(int memid, void *dst, void *src, size_t size,
                                int target, process_config& config)
    {
        put_nbi(dst, src, size, target, config);
    }

    void comm_base::get_nbi(void *dst, void *src, size_t size, int target,
                            process_config& config)
    {
        ep_.rdma_get(dst, src, size, target);
    }

    void comm_base::reg_get_nbi(int memid, void *dst, void *src, size_t size,
                                int target, process_config& config)
    {
        get_nbi(dst, src, size, target, config);
    }

    int comm_base::poll(int *tag_out, int *pid_out, process_config& config)
    {
        return 0;
    }

    void comm_base::fence()
    {
        // do nothing
    }

    void comm_base::native_barrier(process_config& config)
    {
        MPI_Comm comm = config.comm();
        MPI_Barrier(comm);
    }

    template <class T>
    void comm_base::put_value(T *dst, T value, int target,
                              process_config& config)
    {
        static_assert(sizeof(T) == sizeof(uint32_t) ||
                      sizeof(T) == sizeof(uint64_t),
                      "T must be a 32 or 64 bit type");

        auto src = reinterpret_cast<T *>(value_buf_);

        *src = value;
        ep_.rdma_put(dst, src, sizeof(T), target);
    }

    template <class T>
    T comm_base::get_value(T *src, int target, process_config& config)
    {
        static_assert(sizeof(T) == sizeof(uint32_t) ||
                      sizeof(T) == sizeof(uint64_t),
                      "T must be a 32 or 64 bit type");

        auto dst = reinterpret_cast<T *>(value_buf_);

        ep_.rdma_get(dst, src, sizeof(T), target);

        return *value_buf_;
    }

    template <class T>
    T comm_base::fetch_and_add(T *dst, T value, int target,
                               process_config& config)
    {
        static_assert(sizeof(T) == sizeof(uint64_t),
                      "T must be a 64 bit type");

        MADI_CHECK((value & ~0xFFFFFFFFUL) == 0);

        auto result_buf = value_buf_;

        ep_.rdma_fetch_and_add(dst, result_buf, value, target);

        return *reinterpret_cast<T *>(result_buf);
    }

    // template instantiation for put_value
    template void comm_base::put_value(int *, int, int, process_config&);
    template void comm_base::put_value(long *, long, int, process_config&);
    template void comm_base::put_value(unsigned int *, unsigned int,
                                       int, process_config&);
    template void comm_base::put_value(unsigned long *, unsigned long,
                                       int, process_config&);

    // template instantiation for get_value
    template int  comm_base::get_value(int *src, int target,
                                       process_config& config);
    template long comm_base::get_value(long *src, int target,
                                       process_config& config);
    template unsigned int  comm_base::get_value(unsigned int *src, int target,
                                                process_config& config);
    template unsigned long comm_base::get_value(unsigned long *src, int target,
                                                process_config& config);

    // template instantiation for fetch_and_add
    template uint64_t comm_base::fetch_and_add<uint64_t>(uint64_t *,
                                                         uint64_t, int,
                                                         process_config&);
}
}

