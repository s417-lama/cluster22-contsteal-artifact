#ifndef MADI_COMM_MEMORY_SHMEM_INL_H
#define MADI_COMM_MEMORY_SHMEM_INL_H

#include "comm_memory.h"
#include "madm_debug.h"

namespace madi {
namespace comm {

    template <class T>
    inline T * coll_shm_map::translate(T *p, size_t size, int pid)
    {
        switch (addr_type_) {
        case address_type::same:
            return p;

        case address_type::different: {
            MADI_ASSERT(0 <= pid && pid < config_.get_native_n_procs());

            uint8_t *ptr = reinterpret_cast<uint8_t *>(p);
            auto me = config_.get_native_pid();

            auto offset = ptr - shm_maps_[me].addr;
            auto addr = shm_maps_[pid].addr + offset;

            MADI_ASSERT(0 <= offset && offset < size_);

            return reinterpret_cast<T *>(addr);
        }
        default:
            MADI_NOT_REACHED;
        }
    }

    template <class T>
    inline T * comm_memory::translate(int memid, T *p, size_t size, int pid)
    {
        return coll_shm_maps_[memid]->translate(p, size, pid);
    }

}
}

#endif
