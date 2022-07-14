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

    template <class T>
    inline future<T>::future()
        : id_(-1)
        , pid_(madi::PID_INVALID)
    {
    }

    template <class T>
    inline future<T>::future(int id, madi::uth_pid_t pid)
        : id_(id)
        , pid_(pid)
    {
    }

    template <class T>
    inline future<T> future<T>::make()
    {
        madi::worker& w = madi::current_worker();
        return make(w);
    }

    template <class T>
    inline future<T> future<T>::make(madi::worker& w)
    {
        return w.fpool().get<T>();
    }

    template <class T>
    inline void future<T>::set(T& value)
    {
        if (id_ < 0 || pid_ == madi::PID_INVALID)
            MADI_DIE("invalid future");

        madi::worker& w = madi::current_worker();
        w.fpool().fill(*this, value);
    }

    template <class T>
    inline bool future<T>::try_get(T *value)
    {
        madi::worker& w = madi::current_worker();
        return try_get(w, value, 0);
    }

    template <class T>
    inline bool future<T>::try_get(madi::worker& w, T *value, uint64_t t_join_reached)
    {
        if (id_ < 0 || pid_ == madi::PID_INVALID)
            MADI_DIE("invalid future");

        madi::future_pool& pool = w.fpool();

        return pool.synchronize(*this, value, t_join_reached);
    }

    template <class T>
    inline T future<T>::get()
    {
        madi::logger::checkpoint<madi::logger::kind::WORKER_BUSY>();

        madi::worker& w = madi::current_worker();

        uint64_t t_join_reached = 0;

        T value;
        while (!try_get(w, &value, t_join_reached)) {
            if (t_join_reached == 0) {
                // the first call to try_get always happens in child stealing,
                // so we do not pass t_join_reached so that it is counted as an outstanding join.
                t_join_reached = madi::global_clock::get_time();
            }
            w.do_scheduler_work();
        }

        madi::logger::checkpoint<madi::logger::kind::WORKER_JOIN_RESOLVED>();

        return value;
    }

}
}

namespace madi {

    inline dist_spinlock::dist_spinlock(uth_comm& c) :
        c_(c),
        locks_(NULL)
    {
        uth_pid_t me = c.get_pid();

        locks_ = (uint64_t **)c_.malloc_shared(sizeof(uint64_t));
        c.lock_init(locks_[me]);
    }

    inline dist_spinlock::~dist_spinlock()
    {
        c_.free_shared((void **)locks_);
    }

    inline bool dist_spinlock::trylock(uth_pid_t target)
    {
        MADI_DPUTSP2("DIST SPIN LOCK");

        uint64_t *lock = locks_[target];
        return c_.trylock(lock, target);
    }

    inline double random_double()
    {
        return (double)random_int(INT_MAX) / (double)INT_MAX;
    }

    inline void dist_spinlock::lock(uth_pid_t target)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::DIST_SPINLOCK_LOCK>();

        while (!trylock(target)) {
            MADI_UTH_COMM_POLL();
        }

        logger::end_event<logger::kind::DIST_SPINLOCK_LOCK>(bd, target);
    }

    inline void dist_spinlock::unlock(uth_pid_t target)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::DIST_SPINLOCK_UNLOCK>();

        MADI_DPUTSP2("DIST SPIN UNLOCK");

        uth_comm::lock_t *lock = locks_[target];
        c_.unlock(lock, target);

        MADI_DPUTSP2("DIST SPIN UNLOCK DONE");

        logger::end_event<logger::kind::DIST_SPINLOCK_UNLOCK>(bd, target);
    }

    template <class T>
    inline dist_pool<T>::dist_pool(uth_comm& c, int size, int local_buf_size) :
        c_(c),
        size_(size),
        locks_(c),
        idxes_(NULL),
        data_(NULL),
        local_buf_size_(local_buf_size)
    {
        uth_pid_t me = c.get_pid();
        size_t nprocs = c.get_n_procs();

        idxes_ = (uth_comm::lock_t **)c_.malloc_shared(sizeof(uth_comm::lock_t));
        data_ = (T **)c_.malloc_shared(sizeof(T) * size);

        for (size_t i = 0; i < nprocs; i++) {
            local_buf_.push_back(std::vector<T>());
        }

        *idxes_[me] = 0UL;
    }

    template <class T>
    inline dist_pool<T>::~dist_pool()
    {
        c_.free_shared((void **)idxes_);
        c_.free_shared((void **)data_);
    }

    template <class T>
    inline bool dist_pool<T>::empty(uth_pid_t target)
    {
        uth_pid_t me = c_.get_pid();

        uint64_t *idx_ptr = idxes_[target];

        uint64_t idx;
        if (target == me) {
            idx = *idx_ptr;
        } else {
            idx = c_.get_value(idx_ptr, target);
        }

        MADI_ASSERT(0 <= idx && idx < size_);

        return idx == 0;
    }

    template <class T>
    inline bool dist_pool<T>::push_remote(T& v, uth_pid_t target)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::DIST_POOL_PUSH>();

        bool success = true;

        local_buf_[target].push_back(v);
        uint64_t buffered_size = local_buf_[target].size();

        if (buffered_size >= local_buf_size_) {
            locks_.lock(target);

            MADI_DPUTSP2("PUSH REMOTE IDX INCR");

            uint64_t *idx_ptr = idxes_[target];
            uint64_t idx = c_.fetch_and_add(idx_ptr, buffered_size, target);

            MADI_ASSERT(0 <= idx && idx < size_);

            if (idx + buffered_size <= size_) {
                T *buf = data_[target] + idx;

                // v is on a stack registered for RDMA
                c_.put_buffered(buf, local_buf_[target].data(),
                                sizeof(T) * buffered_size, target);

                local_buf_[target].clear();
            } else {
                // pool becomes full
                c_.put_value(idx_ptr, idx, target);
                success = false;
            }

            locks_.unlock(target);
        }

        logger::end_event<logger::kind::DIST_POOL_PUSH>(bd, target);

        return success;
    }

    template <class T>
    inline void dist_pool<T>::begin_pop_local()
    {
        log_bd_ = logger::begin_event<logger::kind::DIST_POOL_POP>();

        uth_pid_t target = c_.get_pid();
        locks_.lock(target);
    }

    template <class T>
    inline void dist_pool<T>::end_pop_local()
    {
        uth_pid_t target = c_.get_pid();
        locks_.unlock(target);

        logger::end_event<logger::kind::DIST_POOL_POP>(log_bd_);
    }

    template <class T>
    inline bool dist_pool<T>::pop_local(T *buf)
    {
        uth_pid_t target = c_.get_pid();

        uint64_t current_idx = *idxes_[target];

        MADI_ASSERT(0 <= current_idx && current_idx < size_);

        if (current_idx == 0)
            return false;

        uint64_t idx = current_idx - 1;
        T *src = data_[target] + idx;
        memcpy(buf, src, sizeof(T));

        *idxes_[target] -= 1;

        return true;
    }

    inline size_t index_of_size(size_t size)
    {
        return 64UL - static_cast<size_t>(__builtin_clzl(size - 1));
    }

    inline future_pool::future_pool() :
        ptr_(0), buf_size_(0), remote_bufs_(NULL), retpools_(NULL)
    {
    }
    inline future_pool::~future_pool()
    {
    }

    inline void future_pool::initialize(uth_comm& c, size_t buf_size)
    {
        int retpool_size = get_env("MADM_FUTURE_POOL_RETPOOL_SIZE", 16 * 1024);
        int retpool_local_buf_size = get_env("MADM_FUTURE_POOL_LOCAL_BUF_SIZE", 3);

        ptr_ = 0;
        buf_size_ = (int)buf_size;

        remote_bufs_ = (uint8_t **)c.malloc_shared(buf_size);
        retpools_ = new dist_pool<retpool_entry>(c, retpool_size,
                retpool_local_buf_size);
    }

    inline void future_pool::finalize(uth_comm& c)
    {
        c.free_shared((void **)remote_bufs_);

        for (size_t i = 0; i < MAX_ENTRY_BITS; i++)
            id_pools_[i].clear();

        delete retpools_;

        ptr_ = 0;
        buf_size_ = 0;
        remote_bufs_ = NULL;
        retpools_ = NULL;
    }

    template <class T>
    inline void future_pool::reset(int id)
    {
        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();

        entry<T> *e = (entry<T> *)(remote_bufs_[me] + id);

        e->done = 0;
    }

    inline void future_pool::move_back_returned_ids()
    {
        retpools_->begin_pop_local();

        size_t count = 0;
        retpool_entry entry;
        while (retpools_->pop_local(&entry)) {
            size_t idx = index_of_size(entry.size);
            id_pools_[idx].push_back(entry.id);

            count += 1;
        }

        retpools_->end_pop_local();

        MADI_DPUTSB1("move back returned future ids: %zu", count);
    }

    template <class T>
    inline madm::uth::future<T> future_pool::get()
    {
        logger::begin_data bd = logger::begin_event<logger::kind::FUTURE_POOL_GET>();

        uth_pid_t me = madi::proc().com().get_pid();

        size_t entry_size = sizeof(entry<T>);
        size_t idx = index_of_size(entry_size);

        int real_size = 1 << idx;

        // move future ids from the return pool to the local pool
        if (id_pools_[idx].empty() && !retpools_->empty(me)) {
            move_back_returned_ids();
        }

        int id;
        if (!id_pools_[idx].empty()) {
            // pop a future id from the local pool
            id = id_pools_[idx].back();
            id_pools_[idx].pop_back();

            reset<T>(id);
        } else if (ptr_ + real_size < buf_size_) {
            // if pool is empty, allocate a future id from ptr_
            id = ptr_;
            ptr_ += real_size;
        } else {
            madi::die("future pool overflow");
        }

        madm::uth::future<T> ret = madm::uth::future<T>(id, me);

        logger::end_event<logger::kind::FUTURE_POOL_GET>(bd, id);

        return ret;
    }

    template <class T>
    inline void future_pool::fill(madm::uth::future<T> f, T& value)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::FUTURE_POOL_FILL>();

        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();
        int fid = f.id_;
        uth_pid_t pid = f.pid_;

        entry<T> *e = (entry<T> *)(remote_bufs_[pid] + fid);

        if (pid == me) {
            e->value.value = value;
#if !MADI_LOGGER_DISABLE
            e->value.t_completed = 0;
#endif
            comm::threadsafe::wbarrier();
            e->done = 1;
        } else {
            entry_value<T> eval;
            eval.value = value;
#if !MADI_LOGGER_DISABLE
            eval.t_completed = global_clock::get_time();
#endif

            // value is on a stack registered for RDMA
            c.put_buffered(&e->value, &eval, sizeof(eval), pid);
            c.put_value(&e->done, 1, pid);
        }

        logger::end_event<logger::kind::FUTURE_POOL_FILL>(bd, pid);
    }

    template <class T>
    inline bool future_pool::synchronize(madm::uth::future<T> f, T *value, uint64_t t_join_reached)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::FUTURE_POOL_SYNC>();

        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();
        int fid = f.id_;
        uth_pid_t pid = f.pid_;

        entry<T> *e = (entry<T> *)(remote_bufs_[pid] + fid);

        int done;
        if (pid == me) {
            done = e->done;
        } else {
            done = c.get_value(&e->done, pid);
        }

        MADI_ASSERT(0 <= fid && fid < buf_size_);

        if (done) {
#if !MADI_LOGGER_DISABLE
            uint64_t t_completed = 0;
#endif

            if (pid == me) {
                comm::threadsafe::rbarrier();
                *value = e->value.value;
#if !MADI_LOGGER_DISABLE
                t_completed = e->value.t_completed;
#endif
            } else {
                entry_value<T> eval;
                c.get_buffered(&eval, &e->value, sizeof(entry_value<T>), pid);
                *value = eval.value;
#if !MADI_LOGGER_DISABLE
                t_completed = eval.t_completed;
#endif
            }

            // return used future ids
            if (pid == me) {
                size_t idx = index_of_size(sizeof(entry<T>));
                id_pools_[idx].push_back(fid);
            } else {
                // return fork-join descriptor to processor pid.
                retpool_entry rpentry = { fid, (int)sizeof(entry<T>) };
                bool success = retpools_->push_remote(rpentry, pid);

                if (success) {
                    MADI_DPUTSR1("push future %d to return pool(%zu)",
                                fid, pid);
                } else {
                    madi::die("future return pool becomes full");
                }
            }

#if !MADI_LOGGER_DISABLE
            if (t_completed != 0 && t_join_reached != 0) {
                uint64_t t_join_ready = std::max(t_completed, t_join_reached);
                logger::begin_data bd_join = logger::begin_event<logger::kind::JOIN_OUTSTANDING>(t_join_ready);
                logger::end_event<logger::kind::JOIN_OUTSTANDING>(bd_join);
            }
#endif
        }

        logger::end_event<logger::kind::FUTURE_POOL_SYNC>(bd, pid);

        return done;
    }
}

#endif
