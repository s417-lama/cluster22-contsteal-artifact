#ifndef MADI_COMM_MEMORY_MPI3_H
#define MADI_COMM_MEMORY_MPI3_H

#include "../process_config.h"
#include "../id_pool.h"
#include "madm_misc.h"
#include <vector>
#include <mpi.h>

namespace madi {
namespace comm {

    // collective (registered) memory region
    class comm_memory : noncopyable {

        int max_procs_per_node_;
        int n_procs_per_node_;

        std::vector<MPI_Win> wins_;
        size_t size_;

        std::vector<uint64_t *> rdma_addrs_;
        size_t rdma_idx_;
        id_pool<int> rdma_ids_;

        uint8_t *region_begin_;
        uint8_t *region_end_;

    public:
        explicit comm_memory(process_config& config);
        ~comm_memory();

        size_t size() const;

        std::vector<MPI_Win>& windows() { return wins_; }

        void translate(int memid, void *p, size_t size, int target,
                       size_t *target_disp, MPI_Win *win);

        void * extend_to(size_t size, process_config& config);

        int coll_mmap(uint8_t *addr, size_t size, process_config& config);
        void coll_munmap(int memid, process_config& config);

    private:
        size_t index_of_memid(int memid) const;
        size_t memid_of_index(int idx) const;
        uint8_t * base_address(int pid) const;
        void * extend(process_config& config);
        void coll_mmap_with_id(int memid, uint8_t *addr, size_t size,
                               process_config& config);
    };

}
}

#endif
