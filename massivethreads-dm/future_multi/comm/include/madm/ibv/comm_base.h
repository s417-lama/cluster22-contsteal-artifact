#ifndef MADI_COMM_BASE_IBV_H
#define MADI_COMM_BASE_IBV_H

#include "comm_memory.h"
#include "../ibv.h"
#include "../process_config.h"
#include "../allocator.h"
#include "madm_misc.h"
#include "madm_debug.h"

#include <cstdint>
#include <cerrno>
#include <vector>

namespace madi {
namespace comm {

    typedef allocator<comm_memory> comm_allocator;

    class aminfo;

    // base communication system for Infiniband Verbs (OFED)
    class comm_base : noncopyable {

        process_config native_config_;
        size_t buffer_size_;
        uint8_t *buffer_;
        ibv::environment env_;
        ibv::prepinned_endpoint ep_;
        comm_memory cm_;
        comm_allocator comm_alc_;
        uint64_t *value_buf_;

    public:
        comm_base(int& argc, char **& argv);
        ~comm_base();

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
                     process_config& config)
        { MADI_UNDEFINED; }

        void reply(int tag, void *p, size_t size, aminfo *info,
                   process_config& config)
        { MADI_UNDEFINED; }
    };

}
}

#endif
