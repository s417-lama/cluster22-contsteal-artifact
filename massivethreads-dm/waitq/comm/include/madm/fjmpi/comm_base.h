#ifndef MADI_COMM_BASE_FJMPI_H
#define MADI_COMM_BASE_FJMPI_H

#include "comm_memory.h"
#include "../ampeer.h"
#include "../process_config.h"
#include "../allocator.h"
#include "madm_misc.h"
#include "madm_debug.h"

#include <cstdint>
#include <cerrno>
#include <vector>

namespace madi {
namespace comm {

    void poll();

    // Completion queue management for Fujitsu MPI
    class rdma_handle : noncopyable {
        int count_;
    public:
        rdma_handle() : count_(0) {}
        ~rdma_handle() {}
        bool done() const { return count_ == 0; }
        void incr() { count_ += 1; }
        void decr() { count_ -= 1; MADI_ASSERT(count_ >= 0); }
        int count() { return count_; }
    };

    typedef allocator<comm_memory> comm_allocator;

    // base communication system for Fujitsu MPI
    class comm_core : noncopyable {

        int tag_;
        rdma_handle *rdma_handle_;
        comm_memory *cmr_;
        comm_allocator *comm_alc_;
        volatile long *value_buf_;
        process_config native_config_;

    public:
        comm_core(int fjmpi_tag);
        ~comm_core();

        process_config& native_config() { return native_config_; }

        void ** coll_malloc(size_t size, process_config& config);
        void coll_free(void **ptrs, process_config& config);
        void * malloc(size_t size, process_config& config);
        void free(void *p, process_config& config);
        int  coll_mmap(uint8_t *addr, size_t size, process_config& config);
        void coll_munmap(int memid, process_config& config);
        void put_nbi(void *dst, void *src, size_t size, int target,
                     process_config& config);
        void reg_put_nbi(int memid, void *dst, void *src, size_t size,
                         int target, process_config& config);
        void get_nbi(void *dst, void *src, size_t size, int target,
                     process_config& config);
        void reg_get_nbi(int memid, void *dst, void *src, size_t size,
                         int target, process_config& config);
        void raw_put(int tag, void *dst, void *src, size_t size, int target,
                     int me);
        void raw_put_ordered(int tag, void *dst, void *src, size_t size,
                             int target, int me);
        void raw_put_with_notice(int tag, void *dst, void *src, size_t size,
                                 int target, int me);
        void raw_put__(int memid, void *dst, void *src, size_t size,
                       int target, int tag, int flags, int me);
        void raw_get(int memid, void *dst, void *src, size_t size,
                     int target, int tag, int flags, int me);
        int  ccpoll(int *tag_out, int *pid_out, process_config& config);
        void ccfence();
        void native_barrier(process_config& config);

        template <class T>
        void put_value(T *dst, T value, int target, process_config& config)
        {
            *(T *)value_buf_ = value;

            put_nbi(dst, (void *)value_buf_, sizeof(T), target, config);
            ccfence();
        }

        template <class T>
        T get_value(T *src, int target, process_config& config)
        {
            MADI_DEBUG3({
                *value_buf_ = 0xFFFFFFFFFFFFFFFF;
            });

            get_nbi((void *)value_buf_, src, sizeof(T), target, config);
            ccfence();

            return *(T *)value_buf_;
        }
    };

    class comm_base
        : public comm_core
        , public ampeer<comm_base>
    {
    public:
        comm_base(int& argc, char **& argv, amhandler_t handler);
        ~comm_base() = default;

        void fence();
        int  poll(int *tag_out, int *pid_out, process_config& config);
    };

}
}

#endif
