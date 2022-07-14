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

    class future_pool : noncopyable {

        enum constants {
            MAX_ENTRY_BITS = 16,
        };

        template <class T>
        struct entry {
            T value;
            int resume_flag; // 0/1/2: resume flag, 3/4: freed
            suspended_entry s_entry;
        };

        int locally_freed_val_  = 3;
        int remotely_freed_val_ = 4;

        int ptr_;
        int buf_size_;
        uint8_t **remote_bufs_;

        std::vector<int> id_pools_[MAX_ENTRY_BITS];
        std::vector<int> all_allocated_ids_[MAX_ENTRY_BITS];

        bool forward_ret_;
        uint8_t *forward_buf_;

        logger::begin_data sync_bd_;

        logger::begin_data join_bd_;
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

        template <class T>
        void return_future_id(madm::uth::future<T> f);
    };

}

#endif
