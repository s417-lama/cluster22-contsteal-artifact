#ifndef MADI_UTH_INL_H
#define MADI_UTH_INL_H

#include "debug.h"
#include "process-inl.h"

namespace madm {
namespace uth {

    inline void poll()
    {
        MADI_UTH_COMM_POLL();
        madi::proc().call_user_poll();
    }

    inline long tick()
    {
        return madi::rdtsc();
    }

    inline double time()
    {
        return madi::now();
    }

    template <class F, class... Args>
    inline void start(F f, int& argc, char **& argv, Args... args)
    {
        madi::process& p = madi::proc();
        
        p.initialize(argc, argv);

        p.start(f, argc, argv, args...);

        p.finalize();
    }

}
}

#endif
