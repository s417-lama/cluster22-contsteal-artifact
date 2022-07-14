#ifndef MADI_COMM_FJMPI_H
#define MADI_COMM_FJMPI_H

#include "madm_misc.h"
#include "id_pool.h"
#include "comm_base.h"
#include "ampeer.h"

#include <cstdio>
#include <vector>

namespace madi {
namespace comm {

    class collectives;
    class ampeer_base;
    class ampeer_wrapper;

    enum {
        AM_FETCH_AND_ADD_INT_REQ = AM_IMPLICIT_REPLY + 1,
        AM_FETCH_AND_ADD_INT_REP,
        AM_FETCH_AND_ADD_LONG_REQ,
        AM_FETCH_AND_ADD_LONG_REP,
    };

    struct globals {
        MADI_NONCOPYABLE(globals);

        int server_mod;

        process_config *config_native;
        process_config *config_compute;

        comm_base *cb;
        comm_wrapper *comm_native;     // MPI_COMM_WORLD
        comm_wrapper *comm_compute;    // only compute processes
        comm_wrapper *comm;

        collectives *coll_native;
        collectives *coll_compute;
        collectives *coll;

        ampeer_base *peer_base;
        ampeer_wrapper *peer_native;
        ampeer_wrapper *peer_compute;
        ampeer_wrapper *peer;
        FILE *debug_out;
        int debug_pid;

        bool initialized;

    public:
        globals();
        ~globals();
        void init();
        void finalize();
    };

    extern globals g;

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
    T ** coll_rma_malloc(size_t size)
    {
        return g.comm->coll_malloc<T>(size);
    }

    template <class T>
    void coll_rma_free(T **ptrs)
    {
        g.comm->coll_free(ptrs);
    }

    template <class T>
    T * rma_malloc(size_t size)
    {
        return g.comm->malloc<T>(size);
    }

    template <class T>
    void rma_free(T *p)
    {
        g.comm->free(p);
    }

    template <class T>
    void put_value(T *dst, T value, pid_t target)
    {
        g.comm->put_value(dst, value, target);
    }

    template <class T>
    T get_value(T *src, pid_t target)
    {
        return g.comm->get_value(src, target);
    }

    template <class T> struct fetch_and_add_tag;
    template <> struct fetch_and_add_tag<int>
    { enum { value = AM_FETCH_AND_ADD_INT_REQ }; };
    template <> struct fetch_and_add_tag<long>
    { enum { value = AM_FETCH_AND_ADD_LONG_REQ }; };

    template <class T>
    T fetch_and_add(T *dst, T value, pid_t target)
    {
        T result = *dst;

        *dst += value;

        return result;
    }

    void reduce_long(long dst[], const long src[], size_t size, pid_t root, 
                     reduce_op op);

    template <>
    inline void reduce<long>(long dst[], const long src[], size_t size,
                             pid_t root, reduce_op op)
    {
        dst[0] = src[0];
    }

}
}

#endif
