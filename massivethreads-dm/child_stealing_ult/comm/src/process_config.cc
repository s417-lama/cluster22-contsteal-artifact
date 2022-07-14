#include "process_config.h"
#include "madm_debug.h"
#include <mpi.h>

namespace madi {
namespace comm {

    // GCC 4.6.1 does not support delegating constructors
    void process_config::constr(MPI_Comm comm, func_type native,
                                func_type abstract)
    {
        native_of_abst_ = native;
        abst_of_native_ = abstract;

        MPI_Comm_rank(MPI_COMM_WORLD, &native_pid_);
        MPI_Comm_size(MPI_COMM_WORLD, &native_n_procs_);

        abst_pid_ = abst_of_native_(native_pid_);
        abst_n_procs_ = abst_of_native_(native_n_procs_ - 1) + 1;

//         MPI_Comm_rank(comm, &abst_pid_);
//         MPI_Comm_size(comm, &abst_n_procs_);
    }

    process_config::process_config(MPI_Comm comm, func_type native,
                                   func_type abstract)
        : comm_(comm)
    {
        constr(comm, native, abstract);
    }

    process_config::process_config()
        : comm_(MPI_COMM_WORLD)
    {
        auto identity = [](int v) { return v; };

        constr(MPI_COMM_WORLD, identity, identity);
    }

}
}
