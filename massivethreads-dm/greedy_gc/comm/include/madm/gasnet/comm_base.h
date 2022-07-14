#ifndef MADI_COMM_BASE_GASNET_H
#define MADI_COMM_BASE_GASNET_H

#include "comm_memory.h"
#include "../ampeer.h"
#include "../process_config.h"
#include "../allocator.h"
#include "madm_misc.h"
#include "madm_debug.h"

#include <cstdint>
#include <cerrno>
#include <vector>

#define MADI_GASNET_USE_IBV_ATOMIC 0

#if MADI_GASNET_USE_IBV_ATOMIC
#include "../ibv.h"
#endif

namespace madi {
namespace comm {

    void poll();

    struct poll_thread_arg {
        volatile int done;
    public:
        poll_thread_arg() : done(0) {}
    };

    typedef allocator<comm_memory> comm_allocator;

    // base communication system for GASNet
    class comm_base : noncopyable {

        int tag_;
        comm_memory *cm_;
        comm_allocator *comm_alc_;
        unique_ptr<process_config> native_config_;
        amhandler_t handler_;

#if MADI_GASNET_USE_IBV_ATOMIC
        ibv::environment *env_;
        ibv::prepinned_endpoint *ep_;
        uint64_t *value_buf_;
#else
        pthread_t poll_thread_;
        poll_thread_arg poll_arg_;
#endif

    public:
        comm_base(int& argc, char **& argv, amhandler_t handler);
        ~comm_base();

        process_config& native_config() { return *native_config_; }
        void amhandle(int tag, int pid, void *data, size_t size, aminfo *info)
        { if (handler_ != nullptr) handler_(tag, pid, data, size, info); }

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
//         void raw_put(int tag, void *dst, void *src, size_t size, int target,
//                      int me);
//         void raw_put_ordered(int tag, void *dst, void *src, size_t size,
//                              int target, int me);
//         void raw_put_with_notice(int tag, void *dst, void *src, size_t size,
//                                  int target, int me);
        void raw_put__(int memid, void *dst, void *src, size_t size,
                       int target, int flags, int me);
        void raw_get(int memid, void *dst, void *src, size_t size,
                     int target, int flags, int me);
        int  poll(int *tag_out, int *pid_out, process_config& config);
        void fence();
        void native_barrier(process_config& config);

        template <class T>
        void put_value(T *dst, T value, int target, process_config& config);

        template <class T>
        T get_value(T *src, int target, process_config& config);

        template <class T>
        T fetch_and_add(T *dst, T value, int target, process_config& config);

        void request(int tag, void *p, size_t size, int pid,
                     process_config& config);

        void reply(int tag, void *p, size_t size, aminfo *info,
                   process_config& config);
    };

}
}

#endif
