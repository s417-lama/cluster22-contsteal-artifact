#ifndef MADI_PROCESS_CONFIG_H
#define MADI_PROCESS_CONFIG_H

#include "options.h"
#include "madm_misc.h"
#include "madm_debug.h"
#include <mpi.h>

namespace madi {
namespace comm {

    class process_config : noncopyable {
    public:
        typedef int (*func_type)(int);

    private:
        MPI_Comm comm_;
        int abst_pid_;
        int abst_n_procs_;
        int native_pid_;
        int native_n_procs_;
        func_type native_of_abst_;
        func_type abst_of_native_;

    public:
        process_config();
        process_config(MPI_Comm comm, func_type native, func_type abstract);

        ~process_config() = default;

        int get_pid() const
        {
            MADI_ASSERT(abst_pid_ >= 0);
            return abst_pid_;
        }

        int get_n_procs() const
        {
            MADI_ASSERT(abst_n_procs_ > 0);
            return abst_n_procs_;
        }

        int get_native_pid() const
        {
            MADI_ASSERT(native_pid_ >= 0);
            return native_pid_;
        }

        int get_native_n_procs() const
        {
            MADI_ASSERT(native_n_procs_ > 0);
            return native_n_procs_;
        }

        int native_pid(int pid) const   { return native_of_abst_(pid); }
        int abstract_pid(int pid) const { return abst_of_native_(pid); }
        MPI_Comm comm() const           { return comm_; }

        bool is_compute() const
        {
            return abst_pid_ >= 0;
        }

        void barrier() { MPI_Barrier(comm_); }

    private:
        void constr(MPI_Comm comm,
                    func_type native, func_type abstract);
    };

}
}

#endif
