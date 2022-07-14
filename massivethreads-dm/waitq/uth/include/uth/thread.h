#ifndef MADM_UTH_THREAD_H
#define MADM_UTH_THREAD_H

#include "../uth-cxx-decls.h"
#include "future.h"
#include "uni/context.h"

namespace madm {
namespace uth {

    template <class T>
    class thread {

        // shared object
        future<T> future_;
      
    public:
        // constr/destr with no thread
        thread();
        ~thread() = default;

        // constr create a thread
        template <class F, class... Args>
        explicit thread(const F& f, Args... args);

        // copy and move constrs
        thread& operator=(const thread&) = delete;
        thread(thread&& other);  // TODO: implement

        T join();

    private:
        template <class F, class... Args>
        static void start(future<T> fut, F f, Args... args);
    };

    typedef madi::saved_context saved_context;

    template <class F, class... Args>
    static void fork(F&& f, Args... args);

    template <class F, class... Args>
    static void suspend(F f, Args... args);

    static void resume(saved_context *sctx) MADI_UNUSED;

}
}

#endif
