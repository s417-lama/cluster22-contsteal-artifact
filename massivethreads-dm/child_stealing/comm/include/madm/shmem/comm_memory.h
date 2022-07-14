#ifndef MADI_COMM_MEMORY_SHMEM_H
#define MADI_COMM_MEMORY_SHMEM_H

#include "../process_config.h"
#include "madm_misc.h"
#include <vector>

namespace madi {
namespace comm {

    //
    // address type for collective shared memmory
    // 
    enum class address_type {
        same,           // same memory, same address among all processes
        different,      // same address, different memory
    };

    //
    // collectively allocated shared memory map
    //
    class coll_shm_map : noncopyable {
        
        // inter-process shared memory map
        struct shm_map {
            uint8_t *addr;
            int fd;
        };

        // shared memory ID
        int memid_;

        // Process ID -> shared memory map
        std::vector<shm_map> shm_maps_;

        // shared memory size
        size_t size_;

        // address type of the collective shared memory
        address_type addr_type_;

        process_config& config_;

    public:
        coll_shm_map(int memid,
                     const std::vector<uint8_t *>& addrs, size_t size,
                     address_type type, process_config& config);
        coll_shm_map(int memid, uint8_t *addr, size_t size,
                     process_config&  config);
        ~coll_shm_map();

        uint8_t * address(size_t pid) const;
        size_t size() const;
        void * extend_to(size_t size);

        template <class T>
        T * translate(T *p, size_t size, int pid);
    private:
        void extend_shmem_region(int fd, uint8_t *addr, size_t offset,
                                 size_t size, int idx);
    };

    //
    // collective, registered memory
    //
    class comm_memory : noncopyable {

        int me_;
        int n_procs_;

        // (base address of shared memory, file descriptor) pair
        // at each process withhin a node
        std::vector<std::unique_ptr<coll_shm_map>> coll_shm_maps_;

        uint8_t *region_begin_;
        uint8_t *region_end_;

    public:
        static constexpr int MEMID_DEFAULT = 0;

        explicit comm_memory(process_config& config);
        ~comm_memory() = default;

        template <class T>
        T * translate(int memid, T *p, size_t size, int pid);

        int coll_mmap(uint8_t *addr, size_t size, process_config& config);
        void coll_munmap(int memid, process_config& config);

        size_t size() const;
        void * extend_to(size_t size, process_config& config);

    private:
        void * extend(process_config& config);
        void coll_mmap_with_id(uint8_t *addr, size_t size,
                               int fd, size_t offset, process_config& config);
    };

}
}

#include "comm_memory-inl.h"

#endif
