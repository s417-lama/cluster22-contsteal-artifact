#ifndef MADI_COMM_MEMORY_FJMPI_H
#define MADI_COMM_MEMORY_FJMPI_H

#include "../process_config.h"
#include "../id_pool.h"
#include "madm_misc.h"
#include <vector>
#include <cstdint>

namespace madi {
namespace comm {

    // inter-process shared memory entry
    struct shm_entry {
        uint8_t *addr;
        int fd;
    };

    // collective (registered) memory region
    class comm_memory : noncopyable {

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
        explicit comm_memory(process_config& config);
        ~comm_memory();

        size_t size() const;

        // for extend() region
        uint64_t translate(void *p, size_t size, int pid);
        // for coll_mmap() region
        uint64_t translate(void *p, size_t size, int pid, int memid);

        void * extend_to(size_t size, process_config& config);

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

}
}

#endif
