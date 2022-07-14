#ifndef MADI_COMM_BASE_SHMEM_INL_H
#define MADI_COMM_BASE_SHMEM_INL_H

#include "comm_base.h"
#include "comm_memory.h"
#include "../threadsafe.h"

namespace madi {
namespace comm {

    template <class T>
    inline void comm_base::put_value(T *dst, T value, int target,
                                     process_config& config)
    {
        auto remote_dst = cm_->translate(comm_memory::MEMID_DEFAULT,
                                         dst, sizeof(T), target);

#if 0
        MADI_DP(dst);
        MADI_DP(remote_dst);
#endif

        *remote_dst = value;
        fence();
    }

    template <class T>
    inline T comm_base::get_value(T *src, int target, process_config& config)
    {
        auto remote_src = cm_->translate(comm_memory::MEMID_DEFAULT,
                                         src, sizeof(T), target);

        T result = *remote_src;
        fence();

        return result;
    }

    template <class T>
    inline T comm_base::fetch_and_add(T *dst, T value, int target,
                               process_config& config)
    {
        auto remote_dst = cm_->translate(comm_memory::MEMID_DEFAULT,
                                         dst, sizeof(T), target);

        return threadsafe::fetch_and_add(remote_dst, value);
    }

}
}

#endif
