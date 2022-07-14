#ifndef MADI_TASKQUE_INL_H
#define MADI_TASKQUE_INL_H

#include "taskq.h"
#include "../uth_comm.h"
#include <madm/threadsafe.h>

namespace madi {

    typedef comm::threadsafe threadsafe;

    inline void global_taskque::push(uth_comm& c, const taskq_entry& entry)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::TASKQ_PUSH>();

        local_empty_ = false;

        int t = top_;

        comm::threadsafe::rbarrier();

        if (t == n_entries_) {
            pid_t me = c.get_pid();
            c.lock(&lock_, me);

            if (base_ == 0)
                madi::die("task queue overflow");

            free_stolen_unsafe(c);

            size_t size = sizeof(taskq_entry) * (top_ - base_);
            memmove(&entries_[0], &entries_[base_], size);

            top_ -= base_;
            base_ = 0;
            freed_ = 0;

            t = top_;

            c.unlock(&lock_, me);
        }

        entries_[t] = entry;

        comm::threadsafe::wbarrier();

        top_ = t + 1;

        MADI_DPUTS3("top = %d", top_);

        logger::end_event<logger::kind::TASKQ_PUSH>(bd);
    }

    inline taskq_entry * global_taskque::pop(uth_comm& c)
    {
        if (local_empty_) {
            return NULL;
        }

        logger::begin_data bd = logger::begin_event<logger::kind::TASKQ_POP>();

        taskq_entry *result;

        int t = top_ - 1;
        top_ = t;

        comm::threadsafe::rwbarrier();

        int b = base_;

        if (b <= t) {
            result = &entries_[t];
        } else {
            pid_t me = c.get_pid();

            top_ = t + 1;
            c.lock(&lock_, me);
            top_ = t;
            b = base_;

            free_stolen_unsafe(c);

            if (b < t) {
                result = &entries_[t];
            } else if (b == t) {
                result = &entries_[t];
                top_ = 0;
                base_ = 0;
                freed_ = 0;
                local_empty_ = true;
            } else {
                result = NULL;
                top_ = 0;
                base_ = 0;
                freed_ = 0;
                local_empty_ = true;
            }

            c.unlock(&lock_, me);
        }

        logger::end_event<logger::kind::TASKQ_POP>(bd);

        return result;
    }

    inline bool global_taskque::local_steal(taskq_entry *entry)
    {
        // quick check
        if (top_ - base_ <= 0)
            return false;

        int b = base_;
        base_ = b + 1;

        comm::threadsafe::rwbarrier();

        int t = top_;

        bool result;
        if (b < t) {
            *entry = entries_[b];
            result = true;
        } else {
            base_ = b;
            result = false;
        }

        return result;
    }

    inline bool global_taskque::empty(uth_comm& c, uth_pid_t target,
                                      global_taskque *taskq_buf)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::TASKQ_EMPTY>();

        global_taskque& self = *taskq_buf; // RMA buffer
        c.get(&self, this, sizeof(self), target);

        logger::end_event<logger::kind::TASKQ_EMPTY>(bd, target);

        return self.base_ >= self.top_;
    }

    inline bool global_taskque::trylock(uth_comm& c, uth_pid_t target)
    {
        return c.trylock(&lock_, target);
    }

    inline void global_taskque::unlock(uth_comm& c, uth_pid_t target)
    {
        c.unlock(&lock_, target);
    }

    inline bool global_taskque::steal(uth_comm& c,
                                      uth_pid_t target,
                                      taskq_entry *entries,
                                      taskq_entry *entry,
                                      global_taskque *taskq_buf)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::TASKQ_STEAL>();

        // assume that `this' pointer is remote.
        // assume that this function is protected by
        // steal_trylock and steal_unlock.

        bool result;

        int b = c.fetch_and_add((int *)&base_, 1, target);
        int t = c.get_value((int *)&top_, target);

        if (b < t) {
            MADI_DPUTS3("RDMA_GET(%p, %p, %zu) rma_entries[%d] = %p",
                        entry, &entries[b], sizeof(*entry),
                        target, entries);

            MADI_CHECK(entries != NULL);

            c.get(entry, &entries[b], sizeof(*entry), target);

            MADI_DPUTS3("RDMA_GET done");

            result = true;
        } else {
            c.fetch_and_add((int *)&base_, -1, target);
            result = false;
        }

        logger::end_event<logger::kind::TASKQ_STEAL>(bd, target);

        return result;
    }

    inline void global_taskque::free_stolen_unsafe(uth_comm& c)
    {
        for (int i = freed_; i < base_; i++) {
            c.free_shared_local((void*)entries_[i].taskobj_base);
        }
        freed_ = base_;
    }

    inline void global_taskque::free_stolen(uth_comm& c)
    {
        pid_t me = c.get_pid();

        c.lock(&lock_, me);

        free_stolen_unsafe(c);

        c.unlock(&lock_, me);
    }

}

#endif
