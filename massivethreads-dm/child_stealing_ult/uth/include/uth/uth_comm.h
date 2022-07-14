#ifndef MADI_UTH_COMM_H
#define MADI_UTH_COMM_H

#include "uth_config.h"
#include "madi.h"
#include "debug.h"
#include <madm/madm_comm_config.h>
#include <madm_comm.h>

#include <cstddef>
#include <cstring>

namespace madi {

    class collectives;

    class uth_comm {
        MADI_NONCOPYABLE(uth_comm);

        int rdma_id_;

        collectives *coll_;

        size_t buffer_size_;
        uint8_t *buffer_;

        bool initialized_;

    public:
        uth_comm() : rdma_id_(-1), coll_(NULL),
                     buffer_size_(0), buffer_(NULL),
                     initialized_(false) {}
        ~uth_comm() {}

        bool initialize(int& argc, char**& argv);
        void finalize();

        template <class... Args>
        void start(void (*f)(int argc, char **argv, Args... args),
                   int argc, char **argv, Args... args);
        void exit(int exitcode) MADI_NORETURN;
        
        bool initialized();

        uth_pid_t get_pid();
        size_t get_n_procs();
            
        void **malloc_shared(size_t size);
        void free_shared(void **p);
        void *malloc_shared_local(size_t size);
        void free_shared_local(void *p);

        void put(void *dst, void *src, size_t size, uth_pid_t target);
        void get(void *dst, void *src, size_t size, uth_pid_t target);
        void put_nbi(void *dst, void *src, size_t size, uth_pid_t target);
        void get_nbi(void *dst, void *src, size_t size, uth_pid_t target);
        void put_buffered(void *dst, void *src, size_t size,
                          uth_pid_t target);
        void get_buffered(void *dst, void *src, size_t size,
                          uth_pid_t target);
        void put_value(int *dst, int value, uth_pid_t target);
        void put_value(long *dst, long value, uth_pid_t target);
        void put_value(uint64_t *dst, uint64_t value, uth_pid_t target);
        int  get_value(int *src, uth_pid_t target);
        long get_value(long *src, uth_pid_t target);
        uint64_t get_value(uint64_t *src, uth_pid_t target);
        void swap(int *dst, int *src, uth_pid_t target);
        template <class T>
        T fetch_and_add(T *dst, T value, uth_pid_t target)
        {
            return comm::fetch_and_add(dst, value, target);
        }

        using lock_t = comm::lock_t;

        void lock_init(lock_t* lp);
        bool trylock(lock_t* lp, uth_pid_t target);
        void lock(lock_t* lp, uth_pid_t target);
        void unlock(lock_t* lp, uth_pid_t target);

        void **reg_mmap_shared(void *addr, size_t size);
        void reg_munmap_shared(void **ptrs, void *addr, size_t size);

        void reg_put(void *dst, void *src, size_t size, uth_pid_t target);
        void reg_get(void *dst, void *src, size_t size, uth_pid_t target);

        void barrier();
        bool barrier_try();

        template <class T>
        void broadcast(T* buf, size_t, uth_pid_t root);

        void poll();

        // active messages (experimental)

        enum {
            AM_STEAL_REQ = 128 + 32,
            AM_STEAL_REP,
        };

        typedef madi::comm::aminfo aminfo;

        void amrequest(int tag, void *p, size_t size, int target);
        void amreply(int tag, void *p, size_t size, aminfo *info);
    };

#if MADI_ENABLE_POLLING

#define MADI_UTH_COMM_POLL() \
    madi::proc().com().poll()

#define MADI_UTH_COMM_POLL_AT_CRAETE()              \
    do {                                            \
        if (uth_options.profile) {                  \
            long t0 = rdtsc();                      \
                                                    \
            MADI_UTH_COMM_POLL();                   \
                                                    \
            long t1 = rdtsc();                      \
            g_prof->t_poll_at_create += t1 - t0;    \
        } else {                                    \
            MADI_UTH_COMM_POLL();                   \
        }                                           \
    } while (false)

#else

#define MADI_UTH_COMM_POLL() ((void)0)
#define MADI_UTH_COMM_POLL_AT_CRAETE()   (MADI_UTH_COMM_POLL())

#endif

    inline void uth_comm::put_buffered(void *dst, void *src, size_t size,
                                       uth_pid_t target)
    {
        MADI_ASSERT(size <= buffer_size_);

        memcpy(buffer_, src, size);

        put(dst, buffer_, size, target);
    }

    inline void uth_comm::get_buffered(void *dst, void *src, size_t size,
                                       uth_pid_t target)
    {
        MADI_ASSERT(size <= buffer_size_);

        get(buffer_, src, size, target);

        memcpy(dst, buffer_, size);
    }

}

#endif
