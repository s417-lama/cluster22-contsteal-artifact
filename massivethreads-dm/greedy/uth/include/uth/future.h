#ifndef MADI_FUTURE_H
#define MADI_FUTURE_H

#include "madi.h"
#include "uth/uni/taskq.h"

namespace madi {

    class worker;
    class future_pool;

}

namespace madm {
namespace uth {

    template <class T>
    class future {
        friend class madi::future_pool;
    private:
        int id_;
        madi::uth_pid_t pid_;

    public:
        future();
        ~future() = default;

        static future<T> make();
        static future<T> make(madi::worker& w);

        future& operator=(const future&) = default;

        void set(T& value);

        T get();

    private:
        future(int id, madi::uth_pid_t pid);
    };
}
}

#include <madm_misc.h>
#include <vector>
#include <cstdint>
#include "uth_comm.h"

namespace madi {
    struct suspended_entry {
        uth_pid_t pid;
        uint8_t* base;
        size_t size;
        uint8_t* stack_top;
    };

    class dist_spinlock {
        uth_comm& c_;
        uth_comm::lock_t **locks_;
    public:
        dist_spinlock(uth_comm& c);
        ~dist_spinlock();

        bool trylock(uth_pid_t target);
        void lock(uth_pid_t target);
        void unlock(uth_pid_t unlock);
    };

    template <class T>
    class dist_pool {
        uth_comm& c_;
        uint64_t size_;
        dist_spinlock locks_;
        uint64_t **idxes_;
        T **data_;

        uint64_t local_buf_size_;
        std::vector< std::vector<T> > local_buf_;

        logger::begin_data log_bd_;
    public:
        dist_pool(uth_comm& c, int size, int local_buf_size);
        ~dist_pool();

        bool empty(uth_pid_t target);

        bool push_remote(T& v, uth_pid_t target);

        void begin_pop_local();
        void end_pop_local();
        bool pop_local(T *buf);
    };

    class future_pool : noncopyable {

        enum constants {
            MAX_ENTRY_BITS = 16,
        };

        template <class T>
        struct entry {
            T value;
            int resume_flag;
            suspended_entry s_entry;
        };

        int ptr_;
        int buf_size_;
        uint8_t **remote_bufs_;

        std::vector<int> id_pools_[MAX_ENTRY_BITS];

        struct retpool_entry {
            int id;
            int size;
        };

        dist_pool<retpool_entry> *retpools_;

        bool forward_ret_;
        uint8_t *forward_buf_;

        logger::begin_data sync_bd_;
    public:
        future_pool();
        ~future_pool();

        void initialize(uth_comm& c, size_t n_entries);
        void finalize(uth_comm& c);

        template <class T>
        madm::uth::future<T> get();

        template <class T>
        bool fill(madm::uth::future<T> f, T& value, bool pop_succeed,
                  suspended_entry *se);

        template <class T>
        bool sync(madm::uth::future<T> f, T *value);

        template <class T>
        bool sync_suspended(madm::uth::future<T> f, suspended_entry se);

        template <class T>
        void sync_resume(madm::uth::future<T> f, T *value);

    private:
        template <class T>
        void reset(int id);

        void move_back_returned_ids();

        template <class T>
        void return_future_id(madm::uth::future<T> f);
    };

}

#endif
