#ifndef MADI_ID_POOL_H
#define MADI_ID_POOL_H

#include <vector>

namespace madi {
namespace comm {

    template <class T>
    class id_pool {
        T from_;
        T to_;
        T next_;
        std::vector<T> free_list_;
    public:
        id_pool(T from, T to) : 
            from_(from), to_(to), next_(from), free_list_()
        {
            // touch OS pages
            size_t possible_elems = to - from;
            free_list_.reserve(possible_elems);

            T *raw = free_list_.data();
            for (size_t i = 0; i < possible_elems; i++)
                raw[i] = 0;
        }
        ~id_pool()
        {
        }

        T from() const { return from_; }
        T to() const { return to_; }

        bool valid(int id) const
        {
            return from_ <= id && id < to_;
        }

        T size() const
        {
            return to_ - next_ + free_list_.size();
        }

        bool empty() const
        {
            return size() == 0;
        }

        bool filled() const
        {
            return size() == to_ - from_;
        }

        void push(T id)
        {
            if (id + 1 == next_) {
                next_ -= 1;
            } else {
                free_list_.push_back(id);
            }
        }

        bool pop(T *id)
        {
            if (!free_list_.empty()) {
                *id = free_list_.back();
                free_list_.pop_back();
                return true;
            } else if (next_ < to_) {
                *id = next_++;
                return true;
            } else {
                return false;
            }
        }
    };

}
}

#endif
