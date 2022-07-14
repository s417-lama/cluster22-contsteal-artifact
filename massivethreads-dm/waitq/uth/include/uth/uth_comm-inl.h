#ifndef MADI_UTH_COMM_INL_H
#define MADI_UTH_COMM_INL_H

#include "uth_comm.h"
#include "debug.h"
#include <madm_comm.h>

namespace madi {

    inline bool uth_comm::initialized()
    {
        return initialized_;
    }

    inline uth_pid_t uth_comm::get_pid()
    {
        MADI_ASSERT(initialized_);
        return comm::get_pid();
    }

    inline size_t uth_comm::get_n_procs()
    {
        MADI_ASSERT(initialized_);
        return comm::get_n_procs();
    }

    template <class... Args>
    void uth_comm::start(void (*f)(int argc, char **argv, Args... args),
                         int argc, char **argv, Args... args)
    {
        comm::start(f, argc, argv, args...);
    }

}

#endif
