#ifndef MADI_INL_H
#define MADI_INL_H

#include "madi.h"
#include "uth_comm-inl.h"
#include "process-inl.h"

namespace madi {

    extern process madi_process;

    inline process& proc() { return madi_process; }

    inline worker& current_worker()
    {
        return madi_process.worker_from_id(0);
    }

    inline bool initialized() { return madi::proc().initialized(); }

    template <class T>
    void set_worker_local(T *value)
    {
        madi::current_worker().set_worker_local(value);
    }

    template <class T>
    T * get_worker_local()
    {
        return madi::current_worker().get_worker_local<T>();
    }

}

#endif
