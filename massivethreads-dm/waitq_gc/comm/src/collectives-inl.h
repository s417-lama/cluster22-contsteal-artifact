#ifndef MADI_COLLECTIVES_INL_H
#define MADI_COLLECTIVES_INL_H

#include "collectives.h"
#include "madm_comm-decls.h"
#include "madm_debug.h"

namespace madi {
namespace comm {

    template <class Comm>
    collectives<Comm>::collectives(Comm& c, process_config& config)
        : c_(c)
        , bufs_(NULL)
        , barrier_state_(0)
        , phase_(0), phase0_idx_(0)
        , config_(config)
    {
        int me = config_.get_pid();

        size_t n_elems = 2 * 2;
        bufs_ = (int **)c_.coll_malloc(sizeof(int *) * n_elems, config);

        MADI_CHECK(bufs_ != NULL);

        for (size_t i = 0; i < n_elems; i++)
            bufs_[me][i] = 0;

        parent_      = (me == 0) ? -1 : (me - 1) / 2;
        parent_idx_  = (me == 0) ? -1 : (me - 1) % 2;
        children_[0] = 2 * me + 1;
        children_[1] = 2 * me + 2;

        config_.barrier();
    }

    template <class Comm>
    collectives<Comm>::~collectives()
    {
        config_.barrier();

        c_.coll_free((void **)bufs_, config_);
    }

    template <class Comm>
    bool collectives<Comm>::barrier_try()
    {
        int me = config_.get_pid();
        int n_procs = config_.get_n_procs();

        int base = 2 * barrier_state_;

        // reduce
        if (phase_ == 0) {
            for (int i = phase0_idx_; i < 2; i++) {
                if (children_[i] < n_procs) {
                    if (bufs_[me][base + i] != 1) {
                        phase_ = 0;
                        phase0_idx_ = i;
                        return false;
                    }
                }
            }

            if (parent_ != -1) {
                c_.put_value(&bufs_[parent_][base + parent_idx_], 1, 
                             parent_, config_);
            }

            phase_ += 1;
        }

        // broadcast
        if (phase_ == 1) {
            if (parent_ != -1) {
                if (bufs_[me][base + 0] != 2) {
                    phase_ = 1;
                    return false;
                }
            }

            for (int i = 0; i < 2; i++) {
                int child = children_[i];
                if (child < n_procs) {
                    c_.put_value(&bufs_[child][base + 0], 2, child, config_);
                }
            }
        }

        phase_ = 0;
        phase0_idx_ = 0;

        bufs_[me][base + 0] = 0;
        bufs_[me][base + 1] = 0;

        barrier_state_ = !barrier_state_;

        return true;
    }

    template <class Comm>
    void collectives<Comm>::barrier()
    {
        while (!barrier_try())
            madi::comm::poll();
    }

    template <class T> inline MPI_Datatype mpi_type();
    template <> inline MPI_Datatype mpi_type<int          >() { return MPI_INT;           }
    template <> inline MPI_Datatype mpi_type<unsigned int >() { return MPI_UNSIGNED;      }
    template <> inline MPI_Datatype mpi_type<long         >() { return MPI_LONG;          }
    template <> inline MPI_Datatype mpi_type<unsigned long>() { return MPI_UNSIGNED_LONG; }

    template <class Comm>
    template <class T>
    void collectives<Comm>::broadcast(T* buf, size_t size, pid_t root)
    {
#if MADI_COMM_LAYER != MADI_COMM_LAYER_SEQ
        MPI_Comm comm = config_.comm();
        MPI_Datatype mpitype = mpi_type<T>();

        MPI_Bcast(const_cast<T*>(buf), size, mpitype, root, comm);
#endif
    }

    template <class Comm>
    template <class T>
    void collectives<Comm>::reduce(T dst[], const T src[],
                                   size_t size, pid_t root, reduce_op op)
    {
#if MADI_COMM_LAYER == MADI_COMM_LAYER_SEQ
        for (size_t i = 0; i < size_t; i++) dst[i] = src[i];
#else
        // FIXME: collective operation has to call poll() function
        MPI_Comm comm = config_.comm();
        MPI_Datatype mpitype = mpi_type<T>();

        MPI_Op mpi_op;
        switch (op) {
            case reduce_op_sum: mpi_op = MPI_SUM; break;
            case reduce_op_product: mpi_op = MPI_PROD; break;
            case reduce_op_min: mpi_op = MPI_MIN; break;
            case reduce_op_max: mpi_op = MPI_MAX; break;
            default: MADI_NOT_REACHED;
        }

        MPI_Reduce(const_cast<T *>(src), dst, size, mpitype, mpi_op,
                   root, comm);
#endif
    }

}
}

#endif
