#ifndef MADI_COLLECTIVES_H
#define MADI_COLLECTIVES_H

#include "madm/madm_comm-decls.h"
#include "process_config.h"
#include "madm_misc.h"

namespace madi {
namespace comm {

    template <class Comm>
    class collectives : noncopyable {
        Comm& c_;
        int **bufs_; 
        int barrier_state_;

        int parent_;
        int parent_idx_;
        int children_[2];

        int phase_;
        int phase0_idx_;

        process_config& config_;

    public:
        explicit collectives(Comm& c, process_config& config);
        ~collectives();
        bool barrier_try();
        void barrier();

        template <class T>
        void broadcast(T* buf, size_t size, pid_t root);
        template <class T>
        void reduce(T dst[], const T src[], size_t size,
                         pid_t root, reduce_op op);
    };

}
}

#endif
