#include "fjmpi/comm_memory.h"

#include "madm_misc.h"
#include "madm_debug.h"

#include <climits>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <mpi.h>
#include <mpi-ext.h>

namespace madi {
namespace comm {

    enum cmr_constants {
        CMR_BASE_ADDR = 0x30000000000,

        CMR_MAX_BITS = 32,
        CMR_MAX_SIZE = 1UL << CMR_MAX_BITS,

        CMR_PROC_BITS = 8,
        CMR_PROC_SIZE = 1UL << CMR_PROC_BITS,

        CMR_BASE_BITS = 22, // FIXME: temporal fix //13, // 8192 bytes
                            // 4MB = 
                            // serverbuf: 128B * 16 cores * 1K nodes = 2MB
                            // + a
        CMR_BASE_SIZE = 1UL << CMR_BASE_BITS,
    };

    int open_shared_file(int idx, bool creat)
    {
        MADI_ASSERT(idx >= 0);

        int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        int oflag = O_RDWR;

        if (creat)
            oflag |= O_CREAT;

        char fname[NAME_MAX];
        sprintf(fname, "/massivethreadsdm.%d", idx);

        if (creat) {
            int r = shm_unlink(fname);

            if (r != 0 && errno != ENOENT) {
                perror("shm_unlink");
                MADI_CHECK(r == 0);
            }
        }

//        MADI_DPUTS3("shm_open(%s, ...)", fname);

        int fd = shm_open(fname, oflag, mode);

        if (fd < 0) {
            perror("shm_open");
            MADI_CHECK(fd >= 0);
        }

//         if (creat) {
//             int r = ftruncate(fd, size);
//
//             if (r != 0) {
//                 perror("ftruncate");
//                 MADI_CHECK(r == 0);
//             }
//         }

        return fd;
    }

    void close_shared_file(int fd, bool unlink, int idx)
    {
        close(fd);

        if (unlink) {
            char fname[NAME_MAX];
            sprintf(fname, "/massivethreadsdm.%d", idx);

            int r = shm_unlink(fname);

            MADI_CHECK(r == 0);
        }
    }

    comm_memory::comm_memory(process_config& config) :
        max_procs_per_node_(options.n_procs_per_node),
        n_procs_per_node_(0),
        shm_entries_(max_procs_per_node_),
        shm_offset_(0),
        shm_idx_(0),
        rdma_id_base_(256),
        rdma_addrs_(511 - rdma_id_base_, NULL),
        rdma_idx_(0),
        rdma_ids_(rdma_id_base_ + CMR_MAX_BITS - CMR_BASE_BITS, 511),
        region_begin_((uint8_t *)CMR_BASE_ADDR),
        region_end_(region_begin_ + CMR_PROC_SIZE * CMR_MAX_SIZE)
    {
        FJMPI_Rdma_init();

        int me = config.get_native_pid();
        int n_procs = config.get_native_n_procs();


        int aligned_n_procs = n_procs / max_procs_per_node_
                                      * max_procs_per_node_;

        if (me < aligned_n_procs)
            n_procs_per_node_ = max_procs_per_node_;
        else
            n_procs_per_node_ = n_procs % max_procs_per_node_;

        shm_idx_ = me % max_procs_per_node_;
        shm_entry& entry = shm_entries_[shm_idx_];

        entry.addr = (uint8_t *)CMR_BASE_ADDR + CMR_MAX_SIZE * shm_idx_;
        entry.fd = open_shared_file(shm_idx_, true);

        MPI_Barrier(MPI_COMM_WORLD);

        for (int i = 0; i < max_procs_per_node_; i++) {
            if (i != shm_idx_) {
                shm_entry& e = shm_entries_[i];

                e.addr = (uint8_t *)CMR_BASE_ADDR + CMR_MAX_SIZE * i;

                if (i < n_procs_per_node_)
                    e.fd = open_shared_file(i, false);
                else
                    e.fd = -1;
            }
        }

        // initialize for page allocation (vector initializer already done?)
        for (size_t i = 0; i < rdma_addrs_.size(); i++)
            rdma_addrs_[i] = NULL;
    }

    comm_memory::~comm_memory()
    {
        // FIXME: call coll_munmap to deregister [addr, addr + size)

        for (int i = 0; i < n_procs_per_node_; i++) {
            bool unlink = (i == shm_idx_);

            shm_entry& e = shm_entries_[i];

            munmap(e.addr, shm_offset_);
            close_shared_file(e.fd, unlink, i);
        }

        FJMPI_Rdma_finalize();
    }

    size_t comm_memory::index_of_memid(int memid) const
    {
        return memid - rdma_id_base_;
    }

    size_t comm_memory::memid_of_index(int idx) const
    {
        return rdma_id_base_ + idx;
    }

    size_t comm_memory::size() const
    {
        return shm_offset_;
    }

    uint64_t comm_memory::translate(void *p, size_t size, int pid)
    {
        uint8_t *ptr = (uint8_t *)p;
        int pid_shm_idx = pid % max_procs_per_node_;
        uint8_t *base_addr = shm_entries_[pid_shm_idx].addr;

        size_t offset = ptr - base_addr;

        size_t idx, offset2;
        if (offset < CMR_MAX_SIZE) {
            idx = 0;
            offset2 = offset;
        } else {
            size_t bits = 64 - __builtin_clzll(offset);
            idx = bits - CMR_BASE_BITS;
            offset2 = offset - (1ULL << (bits - 1));
        }

#define MADI_SUPPORT_USER_REGIST 1
#if !MADI_SUPPORT_USER_REGIST
        if (idx < 0 || idx >= rdma_idx_)
            MADI_DIE("pointer %p is not registered for RDMA. "
                     "use coll_rma_malloc or rma_malloc.", ptr);

        MADI_ASSERT(rdma_addrs_[idx] != NULL);

        uint64_t addr = rdma_addrs_[idx][pid] + offset2;

        return addr;
#else
        if (region_begin_ <= ptr && ptr + size <= region_end_) {

            MADI_ASSERTP2(0 <= idx && idx < rdma_idx_, idx, rdma_idx_);
            MADI_ASSERTP1(offset2 <= 32 * 1e9, offset2);

            MADI_ASSERT(rdma_addrs_[idx] != NULL);

            uint64_t addr = rdma_addrs_[idx][pid] + offset2;
            return addr;
        } else {
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

            return translate(p, size, pid, memid);
        }
#endif
    }

    uint64_t comm_memory::translate(void *p, size_t size, int pid, int memid)
    {
        int idx = index_of_memid(memid);
        uint64_t *raddrs = rdma_addrs_[idx];

        MADI_ASSERT(0 <= idx && idx < rdma_addrs_.size());
        MADI_ASSERT(raddrs != NULL);

        uint8_t *base_addr = (uint8_t *)raddrs[-3];

        size_t offset = (uint8_t *)p - base_addr;
        uint64_t addr = raddrs[pid] + offset;

        MADI_ASSERT(base_addr != NULL);

        return addr;
    }

    void * comm_memory::extend_to(size_t size, process_config& config)
    {
        if (size < shm_offset_)
            return NULL;

        // FIXME:
        MADI_ASSERT(size <= CMR_MAX_SIZE);

        void *p = extend(config);

        while (shm_offset_ < size)
            extend(config);

        return p;
    }

    template <class T>
    void get_remote_addrs(int memid, T *addrs, process_config& config)
    {
        int me = config.get_pid();
        int n_procs = config.get_n_procs();

#if 1
        for (int i = 0; i < n_procs; i++) {
            if (i == me)
                continue;

            int native_pid = config.native_pid(i);

//            MADI_DPUTS3("FJMPI_Rdma_get_remote_addr(pid=%d, memid=%d)",
//                        native_pid, memid);

            uint64_t remote_addr = FJMPI_Rdma_get_remote_addr(native_pid,
                                                              memid);

            if (remote_addr == FJMPI_RDMA_ERROR)
                die("FJMPI_Rdma_get_remote_addr: error");

            addrs[native_pid] = (T)remote_addr;
        }
#else
        uint64_t rdma_addr = FJMPI_Rdma_get_remote_addr(me_, memid);

        MPI_Allgather(&rdma_addr, sizeof(rdma_addr), MPI_BYTE,
                      addrs, sizeof(rdma_addr) * n_procs_, MPI_BYTE,
                      MPI_COMM_WORLD);
#endif
    }

    void comm_memory::extend_shmem_region(int fd, size_t offset,
                                              size_t size,
                                              process_config& config)
    {
        int r = ftruncate(fd, offset + size);

        MADI_CHECK(r == 0);
    }

    void do_mmap(uint8_t *addr, size_t size, int fd, size_t offset)
    {
        MADI_CHECK((uintptr_t)addr % 8192 == 0);
        MADI_CHECK(size % 8192 == 0);

        int prot = PROT_READ | PROT_WRITE;

        int flags = (fd == -1) ? (MAP_PRIVATE | MAP_ANONYMOUS) : MAP_SHARED;

        if (addr != NULL)
            flags |= MAP_FIXED;

        void *p = mmap(addr, size, prot, flags, fd, offset);

        if (p == MAP_FAILED) {
            MADI_DIE("mmap failed with %s", strerror(errno));
        }
        if (addr != NULL && p != addr) {
            MADI_DPUTS("p = %p, addr = %p, size = %zu", p, addr, size);
            MADI_CHECK(p == addr);
        }

        // touch
        for (size_t i = 0; i < size; i += 8192)
            addr[i] = 0;
    }

    void * comm_memory::extend(process_config& config)
    {
        MPI_Comm comm = config.comm();
        
        shm_entry& entry = shm_entries_[shm_idx_];

        size_t idx = rdma_idx_;

        MADI_CHECK(idx <= CMR_MAX_BITS - CMR_BASE_BITS);

        int memid = memid_of_index(idx);

        size_t base_bits = CMR_BASE_BITS;
        size_t bits = (idx == 0) ? base_bits : base_bits + idx - 1;
        size_t size = 1UL << bits;

        // extend the size of shared memory file
        extend_shmem_region(entry.fd, shm_offset_, size, config);

        MPI_Barrier(comm);

        uint8_t *base_addr = entry.addr + shm_offset_;

        // mmap the region
        coll_mmap_with_id(memid, base_addr, size,
                          entry.fd, shm_offset_, config);

        // mmap the regions in the other process within the current node
        for (int i = 0; i < n_procs_per_node_; i++) {
            if (i != shm_idx_) {
                shm_entry& e = shm_entries_[i];
                do_mmap(e.addr + shm_offset_, size, e.fd, shm_offset_);
            }
        }

        rdma_idx_ += 1;
        shm_offset_ += size;

        return base_addr;
    }

    void comm_memory::coll_mmap_with_id(int memid, uint8_t *addr,
                                            size_t size,
                                            int fd, size_t offset,
                                            process_config& config)
    {
        int me = config.get_native_pid();
        int n_procs = config.get_native_n_procs();
        MPI_Comm comm = config.comm();


        double t0 = now();

        // mmap
        do_mmap(addr, size, fd, offset);

        MADI_DPUTS3("register region [%p, %p) call (size=%zu) to MEMID:%d",
                    addr, addr + size, size, memid);

        double t1 = now();

        // register the region
        uint64_t laddr = FJMPI_Rdma_reg_mem(memid, addr, size);

        double t2 = now();

        MPI_Barrier(comm);

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
        raddrs[me] = laddr;

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

        coll_mmap_with_id(memid, addr, size, -1, 0, config);

        return memid;
    }

    void comm_memory::coll_munmap(int memid, process_config& config)
    {
#if 0
        // deregister the region
        int r = FJMPI_Rdma_dereg_mem(memid);
        if (r != 0) {
            MADI_DPRINT("FJMPI_Rdma_dereg_mem: error");
        }

        rdma_ids_.push(memid);
#endif

        int idx = index_of_memid(memid);

        uint64_t *raddrs = rdma_addrs_[idx];
        void *addr = (void *)raddrs[-3];
        size_t size = (size_t)raddrs[-2];

        MADI_CHECK((int)raddrs[-1] == memid);

        munmap(addr, size);

        delete [] (raddrs - 3);

        rdma_addrs_[idx] = NULL;
    }

}
}
