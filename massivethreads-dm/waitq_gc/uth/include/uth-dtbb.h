
#include "uth/future.h"
#include <madm_debug.h>

namespace madm {
namespace uth {
namespace dtbb {

    // these functions have similar semantics to the functions in Intel TBB

    template <class Iterator, class F>
    void parallel_for(Iterator first, Iterator last, F&& f);

    template <class Index, class F>
    void parallel_for(Index first, Index last, Index grainsize, F&& f);

    // TODO: implement
    template <class Range, class F>
    void parallel_for(const Range& range, F&& f);

    // Iterator: integer is OK
    // FIXME: declaring 'Reduce&&' is reported as a compile error at
    //        gcc-4.7.2 or gcc-4.9.1 (clang can compile it)
    template <class Iterator, class T, class F, class Reduce>
    T parallel_reduce(Iterator first, Iterator last, const T& init, 
                      F&& f, Reduce reduce);

    // TODO: implement
    template <class Range, class T, class F, class Reduce>
    T parallel_reduce(const Range& range, const T&& init, 
                      F&& f, Reduce&& reduce);

    template <class F0, class F1>
    void parallel_invoke(F0&& f0, F1&& f1);

    // TODO: implement
    template <class F0, class F1, class... Fs>
    void parallel_invoke(F0&& f0, F1&& f1, Fs&&... fs);

    template <class F0, class F1, class T, class Reduce>
    T parallel_invoke_reduce(F0&& f0, F1&& f1, T init, Reduce&& reduce);

//     class task_group {
//         class task_link;
//     private:
//         task_link *task_list_;
//
//     public:
//         task_group();
//         ~task_group();
//
//         template <class F>
//         void run(F&& f);
//
//         template <class F>
//         void run_and_wait(F&& f);
//
//         void wait();
//     };

}
}
}

namespace madm {
namespace uth {
namespace dtbb {

    template <class Iterator, class F>
    void parallel_for(Iterator first, Iterator last, F&& f)
    {
        if (last - first == 1) {
            f(first);
        } else {
            auto mid = first + (last - first) / 2;

            thread<void> th([=] { parallel_for(first, mid, f); });
            parallel_for(mid, last, f);

            th.join();
        }
    }

    template <class Index, class F>
    void parallel_for(Index first, Index last, Index grainsize, F&& f)
    {
        if (last - first <= grainsize) {
            f(first, last);
        } else {
            auto mid = (first + last) / 2;

            thread<void> th([=] { parallel_for(first, mid, grainsize, f); });
            parallel_for(mid, last, grainsize, f);

            th.join();
        }
    }

    // Iterator: integer is OK
    // FIXME: declaring 'Reduce&&' is reported as a compile error at
    //        gcc-4.7.2 or gcc-4.9.1 (clang can compile it)
    template <class Iterator, class T, class F, class Reduce>
    T parallel_reduce(Iterator first, Iterator last, const T& init, 
                      F&& f, Reduce reduce) 
    {
        if (last - first == 1) {
            return f(first);
        } else {
            auto mid = first + (last - first) / 2;

            thread<T> th([=] { 
                return parallel_reduce(first, mid, init, f, reduce);
            });
            auto result1 = parallel_reduce(mid, last, init, f, reduce);

            auto result0 = th.join();
            
            return reduce(result0, result1);
        }
    }

    template <class F0, class F1>
    void parallel_invoke(F0&& f0, F1&& f1)
    {
        thread<void> t0(f0);
        f1();
        t0.join();
    }

    template <class F0, class F1, class T, class Reduce>
    T parallel_invoke_reduce(F0&& f0, F1&& f1, T init, Reduce&& reduce)
    {
        thread<T> t0(f0);
        T r1 = f1();
        T r0 = t0.join();

        return reduce(reduce(init, r0), r1);
    }

}
}
}
