#ifndef MADI_COMM_BASE_SEQ_H
#define MADI_COMM_BASE_SEQ_H

#include "madm_misc.h"
#include "madm_debug.h"
#include "allocator.h"
#include "id_pool.h"
#include "sys.h"

#include <cstdint>
#include <cerrno>
#include <vector>

typedef void *MPI_Comm;

namespace madi {
namespace comm {

    void poll();

    class process_config {
        MADI_NONCOPYABLE(process_config);

    public:
        typedef int (*func_type)(int);

    private:
        MPI_Comm comm_;
        int abst_pid_;
        int abst_n_procs_;
        int native_pid_;
        int native_n_procs_;
        func_type native_of_abst_;
        func_type abst_of_native_;

    public:

        process_config(MPI_Comm comm, func_type native, func_type abstract) :
            comm_(comm),
            abst_pid_(-1), abst_n_procs_(0),
            native_pid_(-1), native_n_procs_(0),
            native_of_abst_(native),
            abst_of_native_(abstract)
        {
            abst_pid_ = 0;
            abst_n_procs_ = 1;

            native_pid_ = 0;
            native_n_procs_ = 1;
        }

        ~process_config() {}

        int get_pid() const { return abst_pid_; }
        int get_n_procs() const { return abst_n_procs_; }
        int get_native_pid() const { return native_pid_; }
        int get_native_n_procs() const { return native_n_procs_; }
        int native_pid(int pid) const { return native_of_abst_(pid); }
        int abstract_pid(int pid) const { return abst_of_native_(pid); }
        MPI_Comm comm() { return comm_; }
    };
#if 0
    // inter-process shared memory entry
    struct shm_entry {
        uint8_t *addr;
        int fd;
    };

    // collective (registered) memory region
    class comm_mem_region {
        MADI_NONCOPYABLE(comm_mem_region);

        int max_procs_per_node_;
        int n_procs_per_node_;
        std::vector<shm_entry> shm_entries_;
        size_t shm_offset_;
        int shm_idx_;
        int rdma_id_base_;
        std::vector<uint64_t *> rdma_addrs_;
        size_t rdma_idx_;
        id_pool<int> rdma_ids_;

        uint8_t *region_begin_;
        uint8_t *region_end_;

    public:
        comm_mem_region(process_config& config);
        ~comm_mem_region();

        size_t size() const;

        // for extend() region
        uint64_t calc_rdma_address(void *p, size_t size, int pid);
        // for coll_mmap() region
        uint64_t calc_rdma_address(void *p, size_t size, int pid, int memid);

        void * extend_to(size_t size, void *param);

        int coll_mmap(uint8_t *addr, size_t size, process_config& config);
        void coll_munmap(int memid, process_config& config);

    private:
        size_t index_of_memid(int memid) const;
        size_t memid_of_index(int idx) const;
        void * extend(process_config& config);
        void coll_mmap_with_id(int memid, uint8_t *addr, size_t size,
                               int fd, size_t offset, process_config& config);
        void extend_shmem_region(int fd, size_t offset, size_t size,
                                 process_config& config);
    };

    // Completion queue management for Fujitsu MPI
    class rdma_handle {
        MADI_NONCOPYABLE(rdma_handle);

        int count_;
    public:
        rdma_handle() : count_(0) {}
        ~rdma_handle() {}
        bool done() const { return count_ == 0; }
        void incr() { count_ += 1; }
        void decr() { count_ -= 1; MADI_ASSERT(count_ >= 0); }
        int count() { return count_; }
    };

    typedef allocator<comm_mem_region> comm_allocator;
#endif
    // base communication system for Fujitsu MPI
    class comm_base {
        MADI_NONCOPYABLE(comm_base);
#if 0
        int tag_;
        rdma_handle *rdma_handle_;
        comm_mem_region *cmr_;
        comm_allocator *comm_alc_;
        volatile long *value_buf_;
        int native_n_procs_;
#endif
    public:
        comm_base(int fjmpi_tag, process_config& config)
#if 0
            :
            tag_(fjmpi_tag), rdma_handle_(NULL),
            cmr_(NULL), comm_alc_(NULL),
            value_buf_(NULL),
            native_n_procs_(-1)
#endif
        {
#if 0
            rdma_handle_ = new rdma_handle();
            cmr_ = new comm_mem_region(config);

            // initialize basic RDMA features (malloc/free/put/get)
            comm_alc_ = new allocator<comm_mem_region>(cmr_);

            value_buf_ = (long *)comm_alc_->allocate(sizeof(long), &config);
            native_n_procs_ = config.get_native_n_procs();
#endif
        }

        ~comm_base()
        {
#if 0
            comm_alc_->deallocate((void *)value_buf_);
            delete comm_alc_;
            delete cmr_;
            delete rdma_handle_;
#endif
        }

        void **coll_malloc(size_t size, process_config& config)
        {
#if 0
            int n_procs = config.get_n_procs();
            MPI_Comm comm = config.comm();
            comm_allocator *alc = comm_alc_;

            void **ptrs = new void *[n_procs];

            void *p = alc->allocate(size, &config);

            MADI_ASSERT(p != NULL);

            MPI_Allgather(&p, sizeof(p), MPI_BYTE, 
                          ptrs, sizeof(p), MPI_BYTE,
                          comm);

            int me = config.get_pid();
            MADI_ASSERT(ptrs[me] != NULL);
#endif
            void **ptrs = new void *[1];
            ptrs[0] = std::malloc(size);

            return ptrs;
        }

        void coll_free(void **ptrs, process_config& config)
        {
#if 0
            int me = config.get_pid();
            comm_allocator *alc = comm_alc_;

            alc->deallocate(ptrs[me]);
#endif
            std::free(ptrs[0]);

            delete [] ptrs;

        }

        void *malloc(size_t size, process_config& config)
        {
#if 0
            MPI_Comm comm = config.comm();

            // FIXME: non-collective implementation
            // currently, the comm_allocater::allocate function may call
            // collective function `extend_to'.
            MPI_Barrier(comm);

            comm_allocator *alc = comm_alc_;
            return alc->allocate(size, &config);
#endif
            return std::malloc(size);
        }

        void free(void *p, process_config& config)
        {
#if 0
            MPI_Comm comm = config.comm();

            // FIXME: non-collective implementation
            MPI_Barrier(comm);

            comm_allocator *alc = comm_alc_;
            alc->deallocate(p);
#endif
            std::free(p);
        }

        int coll_mmap(uint8_t *addr, size_t size, process_config& config)
        { mmap(addr, size, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
          return 0;}

        void coll_munmap(int memid, process_config& config)
        { // FIXME 
        }

        void put_nbi(void *dst, void *src, size_t size, int target,
                     process_config& config)
        { MADI_NOT_REACHED; }

        void put_nbi_with_id(int memid, void *dst, void *src, size_t size,
                             int target, process_config& config)
        {
            MADI_NOT_REACHED;
        }

        void get_nbi(void *dst, void *src, size_t size, int target,
                     process_config& config)
        { MADI_NOT_REACHED; }

        void get_nbi_with_id(int memid, void *dst, void *src, size_t size,
                             int target, process_config& config)
        {
            MADI_NOT_REACHED;
        }

        void raw_put(int tag, void *dst, void *src, size_t size, int target,
                     int me)
        { MADI_NOT_REACHED; }

        void raw_put_ordered(int tag, void *dst, void *src, size_t size,
                             int target, int me)
        { MADI_NOT_REACHED; }
        
        void raw_put_with_notice(int tag, void *dst, void *src, size_t size,
                                 int target, int me)
        { MADI_NOT_REACHED; }

        void raw_put__(int memid, void *dst, void *src, size_t size,
                       int target, int tag, int flags, int me)
        { MADI_NOT_REACHED; }

        void raw_get(int memid, void *dst, void *src, size_t size,
                     int target, int tag, int flags, int me)
        { MADI_NOT_REACHED; }

        int poll(int *tag_out, int *pid_out, process_config& config)
        { return 0; }

        void fence()
        {
        }

        void native_barrier(process_config& config)
        {
        }

        template <class T>
        void put_value(T *dst, T value, int target, process_config& config)
        {
            MADI_NOT_REACHED;
        }

        template <class T>
        T get_value(T *src, int target, process_config& config)
        {
            MADI_NOT_REACHED;
        }
    };

    class comm_wrapper {
        MADI_NONCOPYABLE(comm_wrapper);

        comm_base& c_;
        process_config& config_;

    public:
        comm_wrapper(comm_base& c, process_config& config) :
            c_(c), config_(config) {}

        int get_pid() const { return config_.get_pid(); }
        int get_n_procs() const { return config_.get_n_procs(); }

        const process_config& config() const { return config_; }

        template <class T>
        T **coll_malloc(size_t size)
        { return (T **)c_.coll_malloc(sizeof(T) * size, config_); }

        template <class T>
        void coll_free(T **ptrs)
        { c_.coll_free((void **)ptrs, config_); }

        template <class T>
        T *malloc(size_t size)
        { return (T *)c_.malloc(sizeof(T) * size, config_); }

        template <class T>
        void free(T *p)
        { c_.free((void *)p, config_); }

        int coll_mmap(uint8_t *addr, size_t size)
        { return c_.coll_mmap(addr, size, config_); }

        void coll_munmap(int memid)
        { c_.coll_munmap(memid, config_); }

        void put_nbi(void *dst, void *src, size_t size, int target)
        { c_.put_nbi(dst, src, size, target, config_); }

        void put_nbi_with_id(int memid, void *dst, void *src, size_t size,
                             int target)
        { c_.put_nbi_with_id(memid, dst, src, size, target, config_); }
#if 0
        void raw_put(int tag, void *dst, void *src, size_t size,
                              int target)
        { c_.raw_put(tag, dst, src, size, target); }

        void raw_put_with_notice(int tag, void *dst, void *src, size_t size,
                                 int target)
        { c_.raw_put_with_notice(tag, dst, src, size, target); }
#endif
        void get_nbi(void *dst, void *src, size_t size, int target)
        { c_.get_nbi(dst, src, size, target, config_); }

        void get_nbi_with_id(int memid, void *dst, void *src, size_t size,
                             int target)
        { c_.get_nbi_with_id(memid, dst, src, size, target, config_); }

        int poll(int *tag_out, int *pid_out)
        { return c_.poll(tag_out, pid_out, config_); }

        void fence()
        { c_.fence(); }

        void native_barrier()
        { c_.native_barrier(config_); }

        void put(void *dst, void *src, size_t size, int target)
        { put_nbi(dst, src, size, target); fence(); }

        void get(void *dst, void *src, size_t size, int target)
        { get_nbi(dst, src, size, target); fence(); }

        template <class T>
        void put_value(T *dst, T value, int target)
        { c_.put_value(dst, value, target, config_); }

        template <class T>
        T get_value(T *src, int target)
        { return c_.get_value(src, target, config_); }

    };

}
}

#endif
