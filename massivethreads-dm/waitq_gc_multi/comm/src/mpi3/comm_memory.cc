#include "mpi3/comm_memory.h"

#include "sys.h"
#include "madm_misc.h"
#include "madm_debug.h"

#include <climits>
#include <cerrno>
#include <cstring>
#include <mpi.h>

namespace madi {
namespace comm {

#define CMR_BASE_ADDR  (reinterpret_cast<uint8_t *>(0x500000000000))

    enum cmr_constants {
        // Up to 2^28 = 256 MB / process
        CMR_MAX_BITS = 28,
        CMR_MAX_SIZE = 1UL << CMR_MAX_BITS,

        // Up to 2^17 = 131072 processes
        CMR_PROC_BITS = 17,
        CMR_PROC_SIZE = 1UL << CMR_PROC_BITS,
    };

    comm_memory::comm_memory(process_config& config)
        : max_procs_per_node_(options.n_procs_per_node)
        , n_procs_per_node_(0)
        , wins_(256, MPI_WIN_NULL)
        , size_(0)
        , rdma_addrs_(256, NULL)
        , rdma_idx_(0)
        , rdma_ids_(CMR_MAX_BITS - get_env("MADM_COMM_ALLOC_BITS", 22) + 1, 256)
        , region_begin_(CMR_BASE_ADDR)
        , region_end_(region_begin_ + (size_t)CMR_PROC_SIZE * CMR_MAX_SIZE)
    {
        int me = config.get_native_pid();
        int n_procs = config.get_native_n_procs();

        if (n_procs > CMR_PROC_SIZE) {
            madi::die("# of processes is too large");
        }
    }

    comm_memory::~comm_memory()
    {
        for (auto& win : wins_) {
            if (win != MPI_WIN_NULL) {
                MPI_Win_unlock_all(win);
                MPI_Win_free(&win);
            }
        }
    }

    size_t comm_memory::index_of_memid(int memid) const
    {
        return memid;
    }

    size_t comm_memory::memid_of_index(int idx) const
    {
        return idx;
    }

    uint8_t * comm_memory::base_address(int pid) const
    {
        return CMR_BASE_ADDR + (size_t)CMR_MAX_SIZE * pid;
    }

    size_t comm_memory::size() const
    {
        return size_;
    }

    void comm_memory::translate(int memid, void *p, size_t size, int pid,
                                size_t *target_disp, MPI_Win *win)
    {
        uint8_t *ptr = (uint8_t *)p;

        if (region_begin_ <= ptr && ptr + size <= region_end_) {
            // default remote memory region

            MADI_ASSERT(memid == -1);

            uint8_t *base_addr = base_address(pid);

            size_t offset = ptr - base_addr;

            size_t idx, offset2;
            if (offset < CMR_MAX_SIZE) {
                idx = 0;
                offset2 = offset;
            } else {
                /* size_t bits = 64 - __builtin_clzll(offset); */
                /* idx = bits - CMR_BASE_BITS; */
                /* offset2 = offset - (1ULL << (bits - 1)); // FIXME: incorrect */
                madi::die("FIXME");
            }

            MADI_ASSERTP2(0 <= idx && idx < rdma_idx_, idx, rdma_idx_);
            MADI_ASSERTP1(offset2 <= 32 * 1e9, offset2);

            MADI_ASSERT(wins_[idx] != MPI_WIN_NULL);

            *target_disp = offset2;
            *win = wins_[idx];
        } else if (memid != -1) {
            // coll_mmap region

            int idx = index_of_memid(memid);
            uint64_t *raddrs = rdma_addrs_[idx];

            MADI_ASSERT(0 <= idx && idx < rdma_addrs_.size());
            MADI_ASSERT(raddrs != NULL);

            uint8_t *base_addr = (uint8_t *)raddrs[-3];

            MADI_ASSERT(base_addr != NULL);

            size_t offset = (uint8_t *)p - base_addr;
            uint64_t addr = raddrs[pid] + offset;

            MADI_ASSERTP1(offset <= 32 * 1e9, offset);
            MADI_ASSERT(wins_[idx] != MPI_WIN_NULL);

            *target_disp = offset;
            *win = wins_[idx];
        } else {
            // coll_mmap region
            // FIXME: O(n) search

            int memid = -1;
            for (size_t i = rdma_ids_.from(); i < rdma_ids_.to(); i++) {
                size_t idx = index_of_memid(i);
                uint64_t *raddrs = rdma_addrs_[idx];

                if (raddrs == NULL)
                    continue;

                uint8_t *addr = (uint8_t *)raddrs[-3];
                size_t size = (size_t)raddrs[-2];
                int id = (int)raddrs[-1];

                if (addr <= ptr && ptr < addr + size) {
                    memid = id;
                    break;
                }
            }

            if (memid == -1) {
                MADI_DIE("pointer %p is not registered for RDMA. "
                         "use coll_rma_malloc.", ptr);
            }

            translate(memid, p, size, pid, target_disp, win);
        }
    }

    void * comm_memory::extend_to(size_t size, process_config& config)
    {
        if (size < size_)
            return NULL;

        // FIXME:
        /* MADI_ASSERTP1(size <= CMR_MAX_SIZE, size); */
        /* MADI_CHECK(size <= CMR_MAX_SIZE); */

        void *p = extend(size, config);

        return p;
    }

    template <class T>
    void get_remote_addrs(int memid, T *addrs, process_config& config)
    {
        int n_procs = config.get_n_procs();

        for (int i = 0; i < n_procs; i++) {
            int native_pid = config.native_pid(i);

            addrs[native_pid] = 0; // base address is resolved by MPI_Win
        }
    }

    void touch_pages(void *p, size_t size)
    {
        uint8_t *array = reinterpret_cast<uint8_t *>(p);
        size_t page_size = options.page_size;

        for (size_t i = 0; i < size; i += page_size)
            array[i] = 0;
    }

    void do_mmap(uint8_t *addr, size_t size, int fd, size_t offset)
    {
        MADI_CHECK((uintptr_t)addr % options.page_size == 0);
        MADI_CHECK(size % options.page_size == 0);

        int prot = PROT_READ | PROT_WRITE;

        int flags = (fd == -1) ? (MAP_PRIVATE | MAP_ANONYMOUS) : MAP_SHARED;
#if 0
        if (addr != NULL)
            flags |= MAP_FIXED;
#endif
        void *p = mmap(addr, size, prot, flags, fd, offset);

        if (p == MAP_FAILED) {
            MADI_DIE("mmap failed with %s", strerror(errno));
        }

        if (addr != NULL && p != addr) {
            MADI_DPUTS("p = %p, addr = %p, size = %zu", p, addr, size);
            MADI_CHECK(p == addr);
        }

        // touch
        touch_pages(addr, size);
    }

    void * comm_memory::extend(size_t size, process_config& config)
    {
        int me = config.get_native_pid();
        MPI_Comm comm = config.comm();

        size_t idx = rdma_idx_;

        if (idx != 0) {
            MADI_DIE("FIXME: the RDMA region cannot be extended");
        }

        int memid = memid_of_index(idx);

        uint8_t *base_addr = base_address(me) + size_;

        // mmap the region
        coll_mmap_with_id(memid, base_addr, size, config);

        rdma_idx_ += 1;
        size_ += size;

        return base_addr;
    }

    void comm_memory::coll_mmap_with_id(int memid, uint8_t *addr, size_t size,
                                        process_config& config)
    {
        int me = config.get_native_pid();
        int n_procs = config.get_native_n_procs();
        MPI_Comm comm = config.comm();

        double t0 = now();

        // mmap
        do_mmap(addr, size, -1, 0);

        MADI_DPUTS3("register region [%p, %p) call (size=%zu, memid=%d)",
                    addr, addr + size, size, memid);

        double t1 = now();

        // register the region
        MPI_Win win;
        int r0 = MPI_Win_create(addr, size, 1, MPI_INFO_NULL, comm, &win);
        MADI_CHECK(r0 == MPI_SUCCESS);

        int flag;
        int *model;
        MPI_Win_get_attr(win, MPI_WIN_MODEL, &model, &flag);

        if (!flag || *model != MPI_WIN_UNIFIED) {
            MADI_DIE("FIXME: the current implementation assume MPI_WIN_MODEL == MPI_WIN_UNIFIED");
        }

        int r1 = MPI_Win_lock_all(0, win);
        MADI_CHECK(r1 == MPI_SUCCESS);

        wins_[memid] = win;

        double t2 = now();

//        MPI_Barrier(comm);

        double t3 = now();

        // allocate header + data storage
        uint64_t *raddrs_buf = new uint64_t[n_procs + 3];
        uint64_t *raddrs = raddrs_buf + 3;
       
        // fill header fields
        raddrs[-3] = (uint64_t)addr;
        raddrs[-2] = (uint64_t)size;
        raddrs[-1] = (uint64_t)memid;

        double t4 = now();

        // fill local and remote DMA addresses
        get_remote_addrs(memid, raddrs, config);
//        raddrs[me] = addr;

        double t5 = now();

        // store the addreses to the translation table
        int idx = index_of_memid(memid);
        rdma_addrs_[idx] = raddrs;

        if (me == 1) {
            MADI_DPUTS1("size = %9zu, "
                       "mmap = %9.6f, reg = %9.6f, "
                       "bar = %9.6f, remaddr = %9.6f",
                       size, t1 - t0, t2 - t1, t3 - t2, t4 - t3);
        }
    }

    int comm_memory::coll_mmap(uint8_t *addr, size_t size,
                               process_config& config)
    {
        int memid;
        bool ok = rdma_ids_.pop(&memid);

        MADI_CHECK(ok);

        coll_mmap_with_id(memid, addr, size, config);

        return memid;
    }

    void comm_memory::coll_munmap(int memid, process_config& config)
    {
        int idx = index_of_memid(memid);
        uint64_t *raddrs = rdma_addrs_[idx];
        void *addr = (void *)raddrs[-3];
        size_t size = (size_t)raddrs[-2];

        MADI_CHECK((int)raddrs[-1] == memid);

        // deregister the region
        MPI_Win win = wins_[idx];
        MPI_Win_unlock_all(win);
        MPI_Win_free(&win);

        wins_[idx] = MPI_WIN_NULL;

        rdma_ids_.push(memid);

        // free memory
        munmap(addr, size);

        delete [] (raddrs - 3);

        rdma_addrs_[idx] = NULL;
    }

}
}
