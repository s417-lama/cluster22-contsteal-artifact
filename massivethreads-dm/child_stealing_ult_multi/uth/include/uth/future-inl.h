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
        w.fpool().fill(*this, value);
    }

    template <class T, int NDEPS>
    inline bool future<T, NDEPS>::try_get(T *value, int dep_id)
    {
        madi::worker& w = madi::current_worker();
        return try_get(w, value, 0, dep_id);
    }

    template <class T, int NDEPS>
    inline bool future<T, NDEPS>::try_get(madi::worker& w, T *value, uint64_t t_join_reached, int dep_id)
    {
        if (id_ < 0 || pid_ == madi::PID_INVALID)
            MADI_DIE("invalid future");

        madi::future_pool& pool = w.fpool();

        return pool.synchronize(*this, value, t_join_reached, dep_id);
    }

    template <class T, int NDEPS>
    inline T future<T, NDEPS>::get(int dep_id)
    {
        madi::logger::checkpoint<madi::logger::kind::WORKER_BUSY>();

        madi::worker& w = madi::current_worker();

        uint64_t t_join_reached = 0;

        T value;
        while (!try_get(w, &value, t_join_reached, dep_id)) {
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
    }

    template <class T, int NDEPS>
    inline void future_pool::reset(int id)
    {
        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();

        entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[me] + id);

        e->done = 0;
        for (int d = 0; d < NDEPS; d++) {
            e->freed[d] = 0;
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
                entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[me] + id);
                if (is_freed_local(e)) {
                    id_pools_[idx].push_back(id);
                    e->done = 0;
                    for (int d = 0; d < NDEPS; d++) {
                        e->freed[d] = 0;
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
    inline void future_pool::fill(madm::uth::future<T, NDEPS> f, T& value)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::FUTURE_POOL_FILL>();

        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();
        int fid = f.id_;
        uth_pid_t pid = f.pid_;

        entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[pid] + fid);

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

    template <class T, int NDEPS>
    inline bool future_pool::is_freed_local(entry<T, NDEPS> *e) {
        if (!e->done) {
            return false;
        }
        for (int d = 0; d < NDEPS; d++) {
            if (e->freed[d] != locally_freed_val_ &&
                e->freed[d] != remotely_freed_val_) {
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
            e->freed[dep_id] = locally_freed_val_;

            if (is_freed_local(e)) {
                size_t idx = index_of_size(sizeof(entry<T, NDEPS>));
                id_pools_[idx].push_back(fid);
                e->done = 0;
                for (int d = 0; d < NDEPS; d++) {
                    e->freed[d] = 0;
                }
            }
        } else {
            // return fork-join descriptor to processor pid.
            c.put_nbi(&e->freed[dep_id], &remotely_freed_val_, sizeof(remotely_freed_val_), pid);
        }
    }


    template <class T, int NDEPS>
    inline bool future_pool::synchronize(madm::uth::future<T, NDEPS> f, T *value,
                                         uint64_t t_join_reached, int dep_id)
    {
        logger::begin_data bd = logger::begin_event<logger::kind::FUTURE_POOL_SYNC>();

        uth_comm& c = madi::proc().com();
        uth_pid_t me = c.get_pid();
        int fid = f.id_;
        uth_pid_t pid = f.pid_;

        entry<T, NDEPS> *e = (entry<T, NDEPS> *)(remote_bufs_[pid] + fid);

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

            return_future_id(f, dep_id);

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

    template <class T, int NDEPS>
    inline void future_pool::discard(madm::uth::future<T, NDEPS> f, int dep_id)
    {
        return_future_id(f, dep_id);
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
