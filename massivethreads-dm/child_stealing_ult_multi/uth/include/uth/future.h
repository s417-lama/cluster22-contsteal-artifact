#ifndef MADI_FUTURE_H
#define MADI_FUTURE_H

#include "madi.h"

namespace madi {

    class worker;
    class future_pool;

}

namespace madm {
namespace uth {

    template <class T, int NDEPS>
    class future {
        friend class madi::future_pool;
    private:
        int id_;
        madi::uth_pid_t pid_;

    public:
        future();
        ~future() = default;

        static future<T, NDEPS> make();
        static future<T, NDEPS> make(madi::worker& w);

        future& operator=(const future&) = default;

        void set(T& value);

        bool try_get(T *value, int dep_id);
        T get(int dep_id);

        void discard(int dep_id);

    private:
        future(int id, madi::uth_pid_t pid);
        bool try_get(madi::worker& w, T *value, uint64_t t_join_reached, int dep_id);
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

        template <class T, int NDEPS>
        struct entry {
            entry_value<T> value;
            int done; // 0: uncompleted, 1: completed
            int freed[NDEPS]; // 0: not freed, 417/418: freed
        };

        int locally_freed_val_  = 417;
        int remotely_freed_val_ = 418;

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

        template <class T, int NDEPS>
        madm::uth::future<T, NDEPS> get();

        template <class T, int NDEPS>
        void fill(madm::uth::future<T, NDEPS> f, T& value);

        template <class T, int NDEPS>
        bool synchronize(madm::uth::future<T, NDEPS> f, T *value, uint64_t t_join_reached, int dep_id);

        template <class T, int NDEPS>
        void discard(madm::uth::future<T, NDEPS> f, int dep_id);

        void discard_all_futures();

    private:
        template <class T, int NDEPS>
        void reset(int id);

        template <class T, int NDEPS>
        void return_future_id(madm::uth::future<T, NDEPS> f, int dep_id);

        template <class T, int NDEPS>
        bool is_freed_local(entry<T, NDEPS> *e);
    };

}

#endif
