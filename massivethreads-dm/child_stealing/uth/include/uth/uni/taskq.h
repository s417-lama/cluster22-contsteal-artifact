#ifndef MADI_TASKQUE_H
#define MADI_TASKQUE_H

#include "../madi.h"
#include "context.h"
#include "../misc.h"
#include "../debug.h"
#include "../uth_comm.h"
#include <deque>
#include <cstring>
#include <climits>

namespace madi {

    struct taskq_entry {
        uint8_t *taskobj_base;
        size_t taskobj_size;
    };

    class global_taskque {
        MADI_NONCOPYABLE(global_taskque);

        volatile int top_;
        volatile int base_;

        int n_entries_;
        taskq_entry *entries_;

        uth_comm::lock_t lock_;

        bool local_empty_;

        int freed_;

    public:
        global_taskque();
        ~global_taskque();

        void initialize(uth_comm& c, taskq_entry *entries, size_t n_entries);
        void finalize(uth_comm& c);

        void push(uth_comm& c, const taskq_entry& entry);
        taskq_entry * pop(uth_comm& c);

        bool empty(uth_comm& c, uth_pid_t target, global_taskque *taskq_buf);
        bool steal(uth_comm& c, uth_pid_t target, taskq_entry *entries,
                   taskq_entry *entry, global_taskque *taskq_buf);
        bool trylock(uth_comm& c, uth_pid_t target);
        void unlock(uth_comm& c, uth_pid_t target);

        bool local_steal(taskq_entry *entry);

        void free_stolen_unsafe(uth_comm& c);
        void free_stolen(uth_comm& c);
    };

    typedef global_taskque taskque;
}


#endif
