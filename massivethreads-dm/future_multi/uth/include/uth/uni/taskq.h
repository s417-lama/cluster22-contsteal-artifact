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

    // TODO: share the definition with suspended_entry
    struct taskq_entry {
        uth_pid_t pid;
        uint8_t* frame_base;
        size_t frame_size;
        uint8_t* stack_top;
    };

#define MADI_TENTRY_PRINT(level, entry_ptr) \
    do { \
        MADI_UNUSED madi::taskq_entry *e__ = (entry_ptr); \
        MADI_DPUTS##level("(" #entry_ptr ")->frame_base = %p", \
                          e__->frame_base); \
        MADI_DPUTS##level("(" #entry_ptr ")->frame_size = %zu", \
                          e__->frame_size); \
        MADI_DPUTS##level("(" #entry_ptr ")->pid        = %zu", \
                          e__->pid); \
        MADI_DPUTS##level("(" #entry_ptr ")->stack_top  = %p", \
                          e__->stack_top); \
    } while (false)

    class global_taskque {
        MADI_NONCOPYABLE(global_taskque);

        volatile int top_;
        volatile int base_;

        int n_entries_;
        taskq_entry *entries_;

        uth_comm::lock_t lock_;

        bool local_empty_;

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
    };

    typedef global_taskque taskque;
}


#endif
