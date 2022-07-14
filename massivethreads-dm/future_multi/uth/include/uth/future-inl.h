#ifndef MADI_FUTURE_INL_H
#define MADI_FUTURE_INL_H

#include "madi.h"
#include "future.h"
#include "uth_comm-inl.h"
#include "process.h"
#include "prof.h"
#include <madm/threadsafe.h>

namespace madi {

    constexpr uth_pid_t PID_INVALID = static_cast<uth_pid_t>(-1);

}

namespace madm {
namespace uth {

    template <class T, int NDEPS>
    inline future<T, NDEPS>::future()
        : id_(-1)
        , pid_(madi::PID_INVALID)
    {
    }

    template <class T, int NDEPS>
    inline future<T, NDEPS>::future(int id, madi::uth_pid_t pid)
        : id_(id)
        , pid_(pid)
    {
    }

    template <class T, int NDEPS>
    inline future<T, NDEPS> future<T, NDEPS>::make()
    {
        madi::worker& w = madi::current_worker();
        return make(w);
    }

    template <class T, int NDEPS>
    inline future<T, NDEPS> future<T, NDEPS>::make(madi::worker& w)
    {
        return w.fpool().get<T, NDEPS>();
    }

    template <class T, int NDEPS>
    inline void future<T, NDEPS>::set(T& value)
    {
        if (id_ < 0 || pid_ == madi::PID_INVALID)
            MADI_DIE("invalid future");

        madi::worker& w = madi::current_worker();
        madi::uth_comm& c = madi::proc().com();

        // pop the parent thread
        madi::taskq_entry *entry = w.taskq().pop(c);

        bool parent_popped = entry != NULL && entry->stack_top == 0;

        madi::suspended_entry ses[NDEPS];
        w.fpool().fill(*this, value, parent_popped, ses);

        if (parent_popped) {
            MADI_DPUTS3("pop context %p", &entry->frame_base);

            madi::logger::checkpoint<madi::logger::kind::WORKER_THREAD_DIE>();

            // just return to the parent
        } else {
            // call an event handler when parent thread is stolen
            madi::proc().call_parent_is_stolen();

            madi::suspended_entry* next_se = NULL;
            for (int d = 0; d < NDEPS; d++) {
                if (ses[d].stack_top != 0 && next_se == NULL) {
                    next_se = &ses[d];
                    break;
                }
            }

            if (next_se == NULL) {
                // no waiter is found
                if (entry != NULL) {
                    // an evacuated task is popped

                    // TODO: we assume that all of the remaining tasks in the queue are
                    // evacuated tasks and there is no remaining stack in the uni-address region.

                    // resume the popped evacuated task
                    madi::suspended_entry se;
                    se.base      = entry->frame_base;
                    se.size      = entry->frame_size;
                    se.pid       = entry->pid;
                    se.stack_top = entry->stack_top;

                    madi::logger::checkpoint<madi::logger::kind::WORKER_THREAD_DIE>();
                    w.resume_remote_suspended(se);
                } else {
                    // move to the scheduler
                    madi::logger::checkpoint<madi::logger::kind::WORKER_THREAD_DIE>();
                    w.resume_main_task();
                }
            } else {
                // some waiters are found

                if (entry != NULL) {
                    // return the popped evacuated task to the queue again
                    w.taskq().push(c, *entry);
                }

                // push the waiters to the local task queue
                for (int d = 0; d < NDEPS; d++) {
                    if (ses[d].stack_top != 0 && &ses[d] != next_se) {
                        madi::taskq_entry te;
                        te.frame_base = ses[d].base;
                        te.frame_size = ses[d].size;
                        te.pid        = ses[d].pid;
                        te.stack_top  = ses[d].stack_top;

                        w.taskq().push(c, te);
                    }
                }

                // resume the first waiter
                madi::logger::checkpoint<madi::logger::kind::WORKER_THREAD_DIE>();
                w.resume_remote_suspended(*next_se);
            }
        }
    }

    template <class T, int NDEPS>
    static void future_join_suspended(madi::saved_context *sctx, future<T, NDEPS> f, int dep_id)
    {
        madi::worker& w = madi::current_worker();
        madi::uth_comm& c = madi::proc().com();
        madi::uth_pid_t me = c.get_pid();

        madi::suspended_entry se;
        se.pid        = me;
        se.base       = (uint8_t *)sctx;
        se.size       = offsetof(madi::saved_context, partial_stack) + sctx->stack_size;
        se.stack_top  = sctx->stack_top;

        if (w.fpool().sync_suspended(f, se, dep_id)) {
            // return to the suspended thread again
            w.resume(sctx);
        } else {
            madi::taskq_entry *entry = w.taskq().pop(c);

            madi::logger::checkpoint<madi::logger::kind::WORKER_JOIN_UNRESOLVED>();

            if (entry != NULL) {
                if (entry->stack_top == 0) {
                    // the parent task is popped
                    madi::context* ctx = (madi::context*)entry->frame_base;
                    MADI_RESUME_CONTEXT(ctx);
                } else {
                    // an evacuated task is popped
                    madi::suspended_entry se;
                    se.base      = entry->frame_base;
                    se.size      = entry->frame_size;
                    se.pid       = entry->pid;
                    se.stack_top = entry->stack_top;

                    w.resume_remote_suspended(se);
                }
            } else {
                // move to the scheduler
                w.resume_main_task();
            }
        }

        MADI_NOT_REACHED;
    }

    template <class T, int NDEPS>
    inline T future<T, NDEPS>::get(int dep_id)
    {
        madi::logger::checkpoint<madi::logger::kind::WORKER_BUSY>();

        madi::worker& w = madi::current_worker();

        T value;
        if (w.is_main_task()) {
            while (!w.fpool().sync(*this, &value, dep_id)) {
                w.do_scheduler_work();
            }
        } else {
            if (!w.fpool().sync(*this, &value, dep_id)) {
                w.suspend(future_join_suspended<T, NDEPS>, *this, dep_id);

                // worker can change after suspend
                madi::worker& w1 = madi::current_worker();

                w1.fpool().sync_resume(*this, &value, dep_id);
            }
        }

        madi::logger::checkpoint<madi::logger::kind::WORKER_JOIN_RESOLVED>();

        return value;
    }

    template <class T, int NDEPS>
    inline void future<T, NDEPS>::discard(int dep_id)
    {
        madi::worker& w = madi::current_worker();
        w.fpool().discard(*this, dep_id);
    }

}
}

namespace madi {

    inline size_t index_of_size(size_t size)
    {
        return 64UL - static_cast<size_t>(__builtin_clzl(size - 1));
    }

    inline future_pool::future_pool() :
        ptr_(0), buf_size_(0), remote_bufs_(NULL)
    {
    }
    inline future_pool::~future_pool()
    {
    }

    inline void future_pool::initialize(uth_comm& c, size_t buf_size)
    {
        ptr_ = 0;
        buf_size_ = (int)buf_size;

        remote_bufs_ = (uint8_t **)c.malloc_shared(buf_size);

        size_t max_value_size = 1 << MAX_ENTRY_BITS;
        forward_buf_ = (uint8_t *)malloc(max_value_size);
        forward_ret_ = false;
    }

    inline void future_pool::finalize(uth_comm& c)
    {
        c.free_shared((void **)remote_bufs_);

        for (size_t i = 0; i < MAX_ENTRY_BITS; i++) {
            id_pools_[i].clear();
            all_allocated_ids_[i].clear();
        }

        ptr_ = 0;
        buf_size_ = 0;
        remote_bufs_ = NULL;

        free(forward_buf_);
    }

    template <class T, int NDEPS>
    inline void future_pool::reset(int id)
    {
        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();

        entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[me] + id);

        for (int d = 0; d < NDEPS; d++) {
            e->resume_flags[d] = 0;
        }
    }

    template <class T, int NDEPS>
    inline madm::uth::future<T, NDEPS> future_pool::get()
    {
        logger::begin_data bd = logger::begin_event<logger::kind::FUTURE_POOL_GET>();

        uth_pid_t me = madi::proc().com().get_pid();

        size_t entry_size = sizeof(entry<T, NDEPS>);
        size_t idx = index_of_size(entry_size);

        int real_size = 1 << idx;

        if (id_pools_[idx].empty()) {
            // collect freed future ids
            for (int id : all_allocated_ids_[idx]) {
                // TODO: it is not guaranteed that all of the allocated ids have the
                // same type and number of dependencies.
                entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[me] + id);

                if (is_freed_local(e)) {
                    id_pools_[idx].push_back(id);
                    for (int d = 0; d < NDEPS; d++) {
                        e->resume_flags[d] = 0;
                    }
                }
            }
        }

        int id;
        if (!id_pools_[idx].empty()) {
            // pop a future id from the local pool
            id = id_pools_[idx].back();
            id_pools_[idx].pop_back();
        } else if (ptr_ + real_size < buf_size_) {
            // if pool is empty, allocate a future id from ptr_
            id = ptr_;
            ptr_ += real_size;
            all_allocated_ids_[idx].push_back(id);
        } else {
            madi::die("future pool overflow");
        }

        reset<T, NDEPS>(id);
        madm::uth::future<T, NDEPS> ret = madm::uth::future<T, NDEPS>(id, me);

        logger::end_event<logger::kind::FUTURE_POOL_GET>(bd, id);

        return ret;
    }

    template <class T, int NDEPS>
    inline void future_pool::fill(madm::uth::future<T, NDEPS> f, T& value,
                                  bool parent_popped, suspended_entry *ses)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::FUTURE_POOL_FILL>();

        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();
        int fid = f.id_;
        uth_pid_t pid = f.pid_;

        entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[pid] + fid);

        if (parent_popped) {
            // fast path
            MADI_ASSERT(pid == me);

            e->value = value;
            for (int d = 0; d < NDEPS; d++) {
                e->resume_flags[d] = 1;
            }
        } else {
            if (pid == me) {
                e->value = value;
            } else {
                // value is on a stack registered for RDMA
                c.put_buffered(&e->value, &value, sizeof(value), pid);
            }

            for (int d = 0; d < NDEPS; d++) {
                int flag = c.fetch_and_add(&e->resume_flags[d], 1, pid);
                if (flag == 0) {
                    // the parent has not reached the join point
                    ses[d].stack_top = 0;
                } else if (flag == 1) {
                    // the parent has already reached the join point and suspended
                    if (pid == me) {
                        ses[d] = e->s_entries[d];
                    } else {
                        // TODO: get them once to optimize performance
                        c.get_buffered(&ses[d], &e->s_entries[d], sizeof(suspended_entry), pid);
                    }

                    if (NDEPS == 1) {
                        // TODO: reconsider for NDEPS > 1
                        // Since this worker resumes the parent, the return value does not
                        // have to be returned via the future entry (forwarding is possible).
                        forward_ret_ = true;
                        *((T*)forward_buf_) = value;
                    }
                } else if (flag == 2) {
                    // this future has been discarded
                    ses[d].stack_top = 0;
                    return_future_id(f, d);
                }
            }
        }

        logger::end_event<logger::kind::FUTURE_POOL_FILL>(bd, pid);
    }

    template <class T, int NDEPS>
    inline bool future_pool::is_freed_local(entry<T, NDEPS> *e) {
        for (int d = 0; d < NDEPS; d++) {
            if (e->resume_flags[d] != locally_freed_val_ &&
                e->resume_flags[d] != remotely_freed_val_) {
                return false;
            }
        }
        return true;
    }

    template <class T, int NDEPS>
    inline void future_pool::return_future_id(madm::uth::future<T, NDEPS> f, int dep_id)
    {
        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();
        int fid = f.id_;
        uth_pid_t pid = f.pid_;

        entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[pid] + fid);
        if (pid == me) {
            e->resume_flags[dep_id] = locally_freed_val_;

            if (is_freed_local(e)) {
                size_t idx = index_of_size(sizeof(entry<T, NDEPS>));
                id_pools_[idx].push_back(fid);
                for (int d = 0; d < NDEPS; d++) {
                    e->resume_flags[d] = 0;
                }
            }
        } else {
            // return fork-join descriptor to processor pid.
            c.put_nbi(&e->resume_flags[dep_id], &remotely_freed_val_, sizeof(remotely_freed_val_), pid);
        }
    }

    template <class T, int NDEPS>
    inline bool future_pool::sync(madm::uth::future<T, NDEPS> f, T *value, int dep_id)
    {
        sync_bd_ = logger::begin_event<logger::kind::FUTURE_POOL_SYNC>();

        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();
        int fid = f.id_;
        uth_pid_t pid = f.pid_;

        entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[pid] + fid);

        int flag;
        if (pid == me) {
            flag = e->resume_flags[dep_id];
        } else {
            flag = c.get_value(&e->resume_flags[dep_id], pid);
        }

        MADI_ASSERT(0 <= fid && fid < buf_size_);

        if (flag > 0) {
            // The target thread has already been completed

            if (pid == me) {
                comm::threadsafe::rbarrier();
                *value = e->value;
            } else {
                c.get_buffered(value, &e->value, sizeof(T), pid);
            }

            return_future_id(f, dep_id);

            logger::end_event<logger::kind::FUTURE_POOL_SYNC>(sync_bd_, pid);

            return true;
        } else {
            // The target thread can be still running, so suspend the current
            // thread and try to acquire resume_flag later
            return false;
        }
    }

    template <class T, int NDEPS>
    inline bool future_pool::sync_suspended(madm::uth::future<T, NDEPS> f,
                                            suspended_entry se, int dep_id)
    {
        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();
        int fid = f.id_;
        uth_pid_t pid = f.pid_;

        entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[pid] + fid);

        // This write should be done before fetch_and_add so that the target
        // can see this write after fetch_and_add by the target
        if (pid == me) {
            e->s_entries[dep_id] = se;
        } else {
            c.put_buffered(&e->s_entries[dep_id], &se, sizeof(suspended_entry), pid);
        }

        bool ret;
        if (c.fetch_and_add(&e->resume_flags[dep_id], 1, pid) == 0) {
            // the target thread is still running, so let the thread resume
            // the current thread when completed

            ret = false;
        } else {
            // the target thread has already been completed

            ret = true;
        }

        logger::end_event<logger::kind::FUTURE_POOL_SYNC>(sync_bd_, pid);

        return ret;
    }

    template <class T, int NDEPS>
    inline void future_pool::sync_resume(madm::uth::future<T, NDEPS> f, T *value, int dep_id)
    {
        if (NDEPS == 1 && forward_ret_) {
            *value = *((T*)forward_buf_);
            forward_ret_ = false;
        } else {
            uth_comm& c = madi::proc().com();
            uth_pid_t me = c.get_pid();
            int fid = f.id_;
            uth_pid_t pid = f.pid_;

            entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[pid] + fid);

            if (pid == me) {
                *value = e->value;
            } else {
                c.get_buffered(value, &e->value, sizeof(T), pid);
            }
        }

        return_future_id(f, dep_id);
    }

    template <class T, int NDEPS>
    inline void future_pool::discard(madm::uth::future<T, NDEPS> f, int dep_id)
    {
        uth_comm& c = madi::proc().com();
        int fid = f.id_;
        uth_pid_t pid = f.pid_;

        entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[pid] + fid);

        // resume_flag = 2 means it is discarded
        if (c.fetch_and_add(&e->resume_flags[dep_id], 2, pid) == 1) {
            // the future has been completed
            return_future_id(f, dep_id);
        }
    }

    inline void future_pool::discard_all_futures()
    {
        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();

        for (size_t idx = 0; idx < MAX_ENTRY_BITS; idx++) {
            id_pools_[idx].clear();
            for (int id : all_allocated_ids_[idx]) {
                id_pools_[idx].push_back(id);
                size_t size = 1 << idx;
                memset(remote_bufs_[me] + id, 0, size);
            }
        }
    }
}

#endif
