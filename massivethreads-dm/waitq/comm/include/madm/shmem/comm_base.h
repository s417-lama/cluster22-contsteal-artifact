#ifndef MADI_COMM_BASE_SHMEM_H
#define MADI_COMM_BASE_SHMEM_H

#include "../process_config.h"
#include "../allocator.h"
#include "madm/madm_comm-decls.h"
#include "madm_misc.h"
#include "madm_debug.h"

#include <cstdint>
#include <mpi.h>

namespace madi {
namespace comm {

    class comm_memory;

    // base communication system for inter-process shared memory
    class comm_base : noncopyable {

        typedef allocator<comm_memory> cm_allocator;

        process_config native_config_;

        // inter-process shared memory manager
        std::unique_ptr<comm_memory> cm_;

        // allocator for inter-process shared memory
        std::unique_ptr<cm_allocator> comm_alc_;

    public:
        explicit comm_base(int& argc, char **& argv, amhandler_t handler);
        ~comm_base() = default;

        process_config& native_config() { return native_config_; }

        void ** coll_malloc(size_t size, process_config& config);
        void coll_free(void **ptrs, process_config& config);

        void * malloc(size_t size, process_config& config);
        void free(void *p, process_config& config);

        int coll_mmap(uint8_t *addr, size_t size, process_config& config);
        void coll_munmap(int memid, process_config& config);

        void put_nbi(void *dst, void *src, size_t size, int target,
                     process_config& config);

        void get_nbi(void *dst, void *src, size_t size, int target,
                     process_config& config);

        void reg_put_nbi(int memid, void *dst, void *src, size_t size,
                         int target, process_config& config);

        void reg_get_nbi(int memid, void *dst, void *src, size_t size,
                         int target, process_config& config);

        int poll(int *tag_out, int *pid_out, process_config& config);

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

    private:
        void do_put(int memid, void *dst, void *src, size_t size,
                    int target, process_config& config);
        void do_get(int memid, void *dst, void *src, size_t size,
                    int target, process_config& config);
    };

}
}

#include "comm_base-inl.h"

#endif
