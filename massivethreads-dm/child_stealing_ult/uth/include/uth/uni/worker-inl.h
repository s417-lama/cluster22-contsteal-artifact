#ifndef MADI_UNI_WORKER_INL_H
#define MADI_UNI_WORKER_INL_H

#include "worker.h"
#include "../uth_config.h"
#include "taskq-inl.h"
#include "context.h"
#include "../debug.h"
#include "../madi.h"
#include "../prof.h"
#include "../madi-inl.h"
#include "../iso_space-inl.h"

extern "C" {
    void madi_worker_do_resume_saved_context(void *p0, void *p1, void *p2,
                                             void *p3);
}

namespace madi {

    template <class F, class... Args>
    struct start_params {
        worker *w;
        F init_f;
        std::tuple<Args...>& args;
        uint8_t *stack;
        size_t stack_size;
    };

    template <class F, class... Args>
    inline void worker_do_start(context *ctx, void *arg0, void *arg1)
    {
        auto& p = *(start_params<F, Args...> *)arg0;

        MADI_CONTEXT_PRINT(2, ctx);

        worker& w = madi::current_worker();

        uint8_t *stack_top;
        MADI_GET_CURRENT_STACK_TOP(&stack_top);

        w.max_stack_usage_ = 0;
        w.stack_bottom_ = stack_top;
        w.cur_ctx_ = ctx;
        w.is_main_task_ = true;

        // execute the start function
        tuple_apply<void>::f(p.init_f, p.args);

        MADI_DPUTS2("user code in a main thread is finished");

        madi::barrier();

        w.cur_ctx_ = NULL;
        w.is_main_task_ = false;
    }

    static __attribute__((noinline)) void make_it_non_leaf() {}

    template <class F, class... Args>
    inline void worker_start(void *arg0, void *arg1, void *arg2, void *arg3)
    {
        auto params = (start_params<F, Args...> *)arg0;

        /* this function call makes worker_start
           look like a non-leaf function.
           this is necessary because otherwise,
           the compiler may generate code that does
           not properly align stack pointer before
           the call instruction done by
           MADI_SAVE_CONTEXT_WITH_CALL. */
        make_it_non_leaf();

        MADI_DPUTS2("worker start");

        auto start = worker_do_start<F, Args...>;
        MADI_SAVE_CONTEXT_WITH_CALL(NULL, start, params, NULL);

        MADI_DPUTS2("worker end");
    }

    template <class F, class... Args>
    inline void worker::start(F f, int argc, char **argv, Args... args)
    {
        // allocate an iso-address stack
        iso_space& ispace = madi::proc().ispace();
        uint8_t *stack = (uint8_t *)ispace.stack();
        size_t stack_size = ispace.stack_size();

        MADI_UNUSED uth_pid_t me = madi::proc().com().get_pid();

        MADI_DPUTS1("pid            = %zu", me);
        MADI_DPUTS1("stack_region   = [%p, %p)", stack, stack + stack_size);
        MADI_DPUTS1("stack_size     = %zu", stack_size);

        std::tuple<int, char **, Args...> argtuple(argc, argv, args...);

        start_params<F, int, char **, Args...> params =
            { this, f, argtuple, stack, stack_size };


        // switch from the default OS thread stack to the iso-address stack,
        // and execute scheduler function
        void (*start)(void *, void *, void *, void *) =
            worker_start<F, int, char **, Args...>;
        MADI_CALL_ON_NEW_STACK(stack, stack_size,
                               start, &params, NULL, NULL, NULL);

        MADI_DPUTS2("context resumed");
    }

    template <class F, class... Args>
    void worker::fork(F f, Args... args)
    {
        uth_comm& c = madi::proc().com();
        worker& w0 = *this;

        size_t size = sizeof(callable_task<F, Args...>);
        void* buf = c.malloc_shared_local(size);
        if (buf == NULL) {
            w0.taskq_->free_stolen(c);
            buf = c.malloc_shared_local(size);
        }
        if (buf == NULL) {
            madi::die("Too small memory for RDMA allocator.");
        }
        callable_task<F, Args...>* t = new (buf) callable_task<F, Args...>(f, args...);

        taskq_entry entry;
        entry.taskobj_base = (uint8_t*)t;
        entry.taskobj_size = size;

        w0.taskq_->push(c, entry);
    }

    inline void worker::exit()
    {
        MADI_NOT_REACHED;
    }

    template <class F, class... Args>
    void worker_do_suspend(context *ctx, void *f_ptr, void *arg_ptr)
    {
        F f = (F)f_ptr;
        std::tuple<context *, Args...> arg =
            *(std::tuple<context *, Args...> *)arg_ptr;

        worker& w0 = madi::current_worker();

        // update the first argument of f from NULL to sctx
        std::get<0>(arg) = ctx;

        // execute a thread start function
        tuple_apply<void>::f(f, arg);

        MADI_NOT_REACHED;
    }

    template <class F, class... Args>
    void worker::suspend(F f, Args... args)
    {
        // suspend the current thread, and create a new thread which
        // executes f(packed_thread, args...).

        worker& w0 = *this;

        void (*fp)(context *, void *, void *) = worker_do_suspend<F, Args...>;

        context *ctx = NULL;
        std::tuple<context *, Args...> arg(ctx, args...);

        // save current state
        MADI_SAVE_CONTEXT_WITH_CALL(NULL, fp, (void *)f, (void *)&arg);

        // call a function registered by at_thread_resuming
        madi::proc().call_thread_resuming();
    }

    inline void worker::resume(context *next_ctx)
    {
        uint8_t *next_stack_top = (uint8_t *)next_ctx->stack_ptr() - 128;
        MADI_EXECUTE_ON_STACK(madi_worker_do_resume_saved_context,
                              next_ctx, NULL, NULL, NULL,
                              next_stack_top);
    }

}

#endif
