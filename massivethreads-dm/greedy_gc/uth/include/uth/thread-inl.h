#ifndef MADM_UTH_THREAD_INL_H
#define MADM_UTH_THREAD_INL_H

#include "future.h"
#include "future-inl.h"
#include "uni/worker-inl.h"

namespace madm {
namespace uth {

    template <class T>
    thread<T>::thread() : future_() {}

    template <class T>
    template <class F, class... Args>
    thread<T>::thread(const F& f, Args... args)
        : future_()
    {
        madi::logger::checkpoint<madi::logger::kind::WORKER_BUSY>();

        madi::worker& w = madi::current_worker();
        future_ = future<T>::make(w);

        w.fork(start<F, Args...>, future_, f, args...);
    }

    template <class T>
    T thread<T>::join()
    {
        return future_.get();
    }

    template <class T>
    template <class F, class... Args>
    void thread<T>::start(future<T> fut, F f, Args... args)
    {
        madi::logger::checkpoint<madi::logger::kind::WORKER_THREAD_FORK>();

        T value = f(args...);

        madi::logger::checkpoint<madi::logger::kind::WORKER_BUSY>();

        fut.set(value);
    }

    template <>
    class thread<void> {
    private:
        future<long> future_;

    public:
        // constr/destr with no thread
        thread()  = default;
        ~thread() = default;

        // constr create a thread
        template <class F, class... Args>
        explicit thread(const F& f, Args... args)
            : future_()
        {
            madi::logger::checkpoint<madi::logger::kind::WORKER_BUSY>();

            madi::worker& w = madi::current_worker();
            future_ = future<long>::make(w);

            w.fork(start<F, Args...>, future_, f, args...);
        }

        // copy and move constrs
        thread& operator=(const thread&) = delete;
        thread(thread&& other);  // TODO: implement

        void join() { future_.get(); }

    private:
        template <class F, class... Args>
        static void start(future<long> fut, F f, Args... args)
        {
            madi::logger::checkpoint<madi::logger::kind::WORKER_THREAD_FORK>();

            f(args...);

            madi::logger::checkpoint<madi::logger::kind::WORKER_BUSY>();

            long value = 0;
            fut.set(value);
        }
    };

    template <class F, class... Args>
    static void fork(F&& f, Args... args)
    {
        madi::logger::checkpoint<madi::logger::kind::WORKER_BUSY>();

        madi::worker& w = madi::current_worker();
        w.fork(f, args...);
    }

    template <class F, class... Args>
    static void suspend(F&& f, Args... args)
    {
        madi::worker& w = madi::current_worker();
        w.suspend(f, args...);
    }

    static void resume(saved_context *sctx)
    {
        madi::worker& w = madi::current_worker();
        w.resume(sctx);
    }

}
}

#endif
