#ifndef MADI_FUTURE_H
#define MADI_FUTURE_H

#include "madi.h"

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

        bool try_get(T *value);
        T get();

    private:
        future(int id, madi::uth_pid_t pid);
        bool try_get(madi::worker& w, T *value, uint64_t t_join_reached);
    };
}
}

#include <madm_misc.h>
#include <vector>
#include <cstdint>
#include "uth_comm.h"

namespace madi {

    class future_pool : noncopyable {
        enum constants {
            MAX_ENTRY_BITS = 16,
        };

        template <class T>
        struct entry_value {
            T value;
#if !MADI_LOGGER_DISABLE
            uint64_t t_completed;
#endif
        };

        template <class T>
        struct entry {
            entry_value<T> value;
            int done; // 0: uncompleted, 1: completed, 2/3: freed
        };

        int locally_freed_val_  = 2;
        int remotely_freed_val_ = 3;

        int ptr_;
        int buf_size_;
        uint8_t **remote_bufs_;

        std::vector<int> id_pools_[MAX_ENTRY_BITS];
        std::vector<int> all_allocated_ids_[MAX_ENTRY_BITS];
    public:
        future_pool();
        ~future_pool();

        void initialize(uth_comm& c, size_t n_entries);
        void finalize(uth_comm& c);

        template <class T>
        madm::uth::future<T> get();

        template <class T>
        void fill(madm::uth::future<T> f, T& value);

        template <class T>
        bool synchronize(madm::uth::future<T> f, T *value, uint64_t t_join_reached);

    private:
        template <class T>
        void reset(int id);
    };

}

#endif
