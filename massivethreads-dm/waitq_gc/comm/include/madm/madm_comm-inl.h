#ifndef MADI_COMM_INL_H
#define MADI_COMM_INL_H

#include "madm_misc.h"
#include "madm_logger.h"
#include "comm_system.h"
#include "id_pool.h"

#include <cstdio>
#include <vector>

namespace madi {
namespace comm {

    struct globals : private noncopyable {
        bool initialized;

        int debug_pid;
        FILE *debug_out;

        comm_system *comm;

    public:
        globals() : initialized(false), debug_pid(-1), debug_out(stderr)
                  , comm(NULL)
        {
        }
        ~globals() = default;
    };

    extern globals g;

    template <class... Args>
    inline void start(void (*f)(int, char **, Args...), int& argc, char **&argv,
                      Args... args)
    {
        g.comm->start(f, argc, argv, args...);
    }

    inline bool initialized()
    {
        return g.initialized;
    }

    inline pid_t get_pid()
    {
        return (pid_t)g.comm->get_pid();
    }

    inline size_t get_n_procs()
    {
        return (size_t)g.comm->get_n_procs();
    }

    template <class T>
    inline T ** coll_rma_malloc(size_t size)
    {
        return g.comm->coll_malloc<T>(size);
    }

    template <class T>
    inline void coll_rma_free(T **ptrs)
    {
        g.comm->coll_free(ptrs);
    }

    template <class T>
    inline T * rma_malloc(size_t size)
    {
        return g.comm->malloc<T>(size);
    }

    template <class T>
    inline void rma_free(T *p)
    {
        g.comm->free(p);
    }

    template <class T>
    inline void put_value(T *dst, T value, pid_t target)
    {
        g.comm->put_value(dst, value, target);
    }

    template <class T>
    inline T get_value(T *src, pid_t target)
    {
        return g.comm->get_value(src, target);
    }

    template <class T>
    inline T fetch_and_add(T *dst, T value, pid_t target)
    {
        return g.comm->fetch_and_add(dst, value, target);
    }

    inline void lock_init(lock_t* lp)
    {
        g.comm->lock_init(lp);
    }

    inline bool trylock(lock_t* lp, int target)
    {
        return g.comm->trylock(lp, target);
    }

    inline void lock(lock_t* lp, int target)
    {
        g.comm->lock(lp, target);
    }

    inline void unlock(lock_t* lp, int target)
    {
        g.comm->unlock(lp, target);
    }

}
}

#endif
