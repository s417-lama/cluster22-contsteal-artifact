#ifndef MADI_COMM_BASE_MPI3_H
#define MADI_COMM_BASE_MPI3_H

#include "comm_memory.h"
#include "../ampeer.h"
#include "../process_config.h"
#include "../allocator.h"
#include "madm_misc.h"
#include "madm_debug.h"
#include "mpi.h"

#include <cstdint>
#include <cerrno>
#include <vector>

namespace madi {
namespace comm {

    void poll();

    using lock_t = uint64_t;
    const MPI_Datatype MPI_LOCK_T = MPI_UINT64_T;

    typedef allocator<comm_memory> comm_allocator;

    // base communication system for Fujitsu MPI
    class comm_base : noncopyable {

        int tag_;
        comm_memory *cmr_;
        comm_allocator *comm_alc_;
        volatile long *value_buf_;
        process_config native_config_;

    public:
        comm_base(int& argc, char **& argv, amhandler_t handler);
        ~comm_base();

        process_config& native_config() { return native_config_; }

        void ** coll_malloc(size_t size, process_config& config);
        void coll_free(void **ptrs, process_config& config);
        void * malloc(size_t size, process_config& config);
        void free(void *p, process_config& config);
        int  coll_mmap(uint8_t *addr, size_t size, process_config& config);
        void coll_munmap(int memid, process_config& config);
        void put(void *dst, void *src, size_t size, int target,
                 process_config& config);
        void reg_put(int memid, void *dst, void *src, size_t size,
                     int target, process_config& config);
        void get(void *dst, void *src, size_t size, int target,
                 process_config& config);
        void reg_get(int memid, void *dst, void *src, size_t size,
                     int target, process_config& config);
        void put_nbi(void *dst, void *src, size_t size, int target,
                     process_config& config);
        void reg_put_nbi(int memid, void *dst, void *src, size_t size,
                         int target, process_config& config);
        void get_nbi(void *dst, void *src, size_t size, int target,
                     process_config& config);
        void reg_get_nbi(int memid, void *dst, void *src, size_t size,
                         int target, process_config& config);
        template <bool BLOCKING>
        void raw_put(int memid, void *dst, void *src, size_t size,
                     int target, int flags, int me);
        template <bool BLOCKING>
        void raw_get(int memid, void *dst, void *src, size_t size,
                     int target, int flags, int me);
        int  poll(int *tag_out, int *pid_out, process_config& config);
        void fence();
        void sync();
        void native_barrier(process_config& config);

        template <class T>
        void put_value(T *dst, T value, int target, process_config& config)
        {
            *(T *)value_buf_ = value;

            put(dst, (void *)value_buf_, sizeof(T), target, config);
        }

        template <class T>
        T get_value(T *src, int target, process_config& config)
        {
            MADI_DEBUG3({
                *value_buf_ = 0xFFFFFFFFFFFFFFFF;
            });

            get((void *)value_buf_, src, sizeof(T), target, config);

            return *(T *)value_buf_;
        }

        template <class T>
        T fetch_and_add(T *dst, T value, int target, process_config& config);

        void lock_init(lock_t* lp, process_config& config);
        bool trylock(lock_t* lp, int target, process_config& config);
        void lock(lock_t* lp, int target, process_config& config);
        void unlock(lock_t* lp, int target, process_config& config);

        void request(int tag, void *p, size_t size, int pid,
                     process_config& config)
        { MADI_UNDEFINED; }

        void reply(int tag, void *p, size_t size, aminfo *info,
                   process_config& config)
        { MADI_UNDEFINED; }
    };

}
}

#endif
