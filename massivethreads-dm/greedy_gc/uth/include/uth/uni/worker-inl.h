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
    void madi_worker_do_resume_remote_suspended(void *p0, void *p1, void *p2,
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

/*
 * this macro must be called in the same function the ctx saved.
 */
#define MADI_THREAD_PACK(ctx_ptr, worker, sctx_ptr)                     \
    do {                                                                \
        context& ctx__ = *(ctx_ptr);                                    \
        void *ip__ = ctx__.instr_ptr();                                 \
        void *sp__ = ctx__.stack_ptr();                                 \
        uint8_t *top__ = ctx__.top_ptr();                               \
                                                                        \
        size_t stack_size__ = ctx__.stack_size();                       \
        size_t size__ = offsetof(saved_context, partial_stack) + stack_size__;\
                                                                        \
        /* copy stack frames to heap */                                 \
        saved_context *sctx__ = (worker).alloc_suspended(size__);       \
        sctx__->is_main_task = (worker).is_main_task_;                  \
        sctx__->ip = ip__;                                              \
        sctx__->sp = sp__;                                              \
        sctx__->ctx = &ctx__;                                           \
        sctx__->stack_top = top__;                                      \
        sctx__->stack_size = stack_size__;                              \
        memcpy(sctx__->partial_stack, top__, stack_size__);             \
                                                                        \
        MADI_DPUTSB2("suspended [%p, %p) (size = %zu)",                 \
                     top__, top__ + stack_size__, stack_size__);        \
                                                                        \
        *(sctx_ptr) = sctx__;                                           \
    } while (false)

    /*
      - スレッドの状態
        - running
        - lightweight-suspended (thread in taskq): thread = { sp }
        - suspended (packed, thread not in taskq): packed_thread = { sp, stack }

      - 設計
        - lightweight-suspended と suspended の違いは、worker 全体の
          コールスタック上にデータがあるか、ヒープなどの領域にコールスタックの
          一部が退避されているか
        - sp から bottom_ptr の間のスタックの状態については、
          lightweight-suspended と suspended の両方について同じとする

      - タスク生成時の lightweight suspend/resume
        - caller-/callee-saved registers を call stack に push
        - RESUMEラベルを push
        - sp を取得

        - taskq に sp を push
        - ユーザ関数呼び出し (テンプレートで inline 展開)
        - taskq から sp を pop
        - pop できなかったら scheduler コードに移行
        - (pop できたらタスク生成関数から return,
           saved registers はまとめて破棄される)
          
        - RESUMEラベル:
        - saved registers を pop
        - (タスク生成関数から return)
      
      - lightweight-suspended threads の pack (at steal)
        - (taskq から thread を steal)
        - bottom_ptr から sp までをヒープにコピー
        - bottom_ptr = sp;

      - resume (タスクを実行していない場合, スケジューラ内)
        - packed stack を worker のコールスタックに書き戻す (iso-address)
          (タスクを実行している状態で resume を実行した場合、
          現在実行しているタスクを壊さずにスタックを書き戻すことができるか？)
        - sp を設定
        - ret 命令により、スタック上に保存した PC (RESUMEラベル) にジャンプ

      - resume (タスクが実行中の場合)
        - resume するタスクのスタック使用範囲を計算
        - worker のコールスタックについて、上記範囲をヒープに退避する
        - resume するタスクの packed stack を worker のコールスタック上に書き戻す
        - sp を設定
        - ret 命令により、スタック上に保存した PC (RESUMEラベル) にジャンプ

      - suspend (at suspend)
        - caller-/callee-saved registers を call stack に push
        - RESUMEラベルを call stack に push
        - sp を取得

        - call stack をヒープにコピー
        - 関数呼び出し k(sp)
        - (NOT REACHED)

        - RESUMEラベル:
        - saved registers を pop
        - (suspend 関数から return)
    */
    template <class F, class... Args>
    void worker_do_fork(context *ctx_ptr, void *f_ptr, void *arg_ptr)
    {
        context& ctx = *ctx_ptr;
        F f = *(F *)f_ptr;
        std::tuple<Args...> arg = *(std::tuple<Args...> *)arg_ptr;

        MADI_CONTEXT_PRINT(3, &ctx);
        MADI_CONTEXT_ASSERT(&ctx);

        worker& w0 = madi::current_worker();
        uth_comm& c = madi::proc().com();

        MADI_DPUTS3("parent_ctx = %p", ctx.parent);

        if (w0.is_main_task_) {
            // the main thread must not be pushed into taskq
            // because the main thread must not be stolen by another
            // process.
            MADI_THREAD_PACK(ctx_ptr, w0, &w0.main_sctx_);

            w0.is_main_task_ = false;

            MADI_DPUTS2("main task saved [%p, %p) (size = %zu)",
                        ctx.stack_ptr(),
                        ctx.stack_ptr() + ctx.stack_size(),
                        ctx.stack_size());
        } else {
            MADI_DPUTS3("push context %p", &ctx);

            // push the current (parent) thread
            taskq_entry entry;
            entry.frame_base = ctx.top_ptr();
            entry.frame_size = ctx.stack_size();
            entry.ctx = &ctx;

            MADI_TENTRY_ASSERT(&entry);

            MADI_ASSERT(entry.frame_size < 128 * 1024);

            w0.taskq_->push(c, entry);
        }

        MADI_UTH_COMM_POLL_AT_CRAETE();

#if MADI_ENABLE_STEAL_PROF
        // calculate stack usage for profiling
        {
            uint8_t *stack_top;
            MADI_GET_CURRENT_STACK_TOP(&stack_top);

            size_t stack_usage = w0.stack_bottom_ - stack_top;
            w0.max_stack_usage_ = std::max(w0.max_stack_usage_, stack_usage);
        }
#endif

        w0.cur_ctx_ = ctx_ptr;

        MADI_DPUTS2("start (ctx = %p)", ctx_ptr);

        // execute a child thread
        tuple_apply<void>::f<F, Args...>(f, arg);

        MADI_DPUTS2("end (ctx = %p)", ctx_ptr);
    }

    template <class F, class... Args>
    void worker::fork(F f, Args... args)
    {
        worker& w0 = *this;
        uth_pid_t rank0 = proc().com().get_pid();

        context *prev_ctx = w0.cur_ctx_;

        MADI_ASSERT((uintptr_t)prev_ctx >= 128 * 1024);

        MADI_DPUTS3("&prev_ctx = %p", &prev_ctx);

        MADI_CONTEXT_PRINT(3, prev_ctx);
        MADI_CONTEXT_ASSERT_WITHOUT_PARENT(prev_ctx);

        void (*fp)(context *, void *, void *) = worker_do_fork<F, Args...>;

        std::tuple<Args...> arg(args...);

        // save the current context to the stack
        // (copy register values to the current stack)
        MADI_SAVE_CONTEXT_WITH_CALL(prev_ctx, fp, (void *)&f, (void *)&arg);

        MADI_DPUTS3("resumed: parent_ctx = %p", prev_ctx);
        MADI_DPUTS3("&prev_ctx = %p", &prev_ctx);

        worker& w1 = madi::current_worker();
        uth_pid_t rank1 = proc().com().get_pid();

        MADI_DPUTS3("&madi_process = %p", &madi_process);

        MADI_CONTEXT_PRINT(3, prev_ctx);
        MADI_CONTEXT_ASSERT_WITHOUT_PARENT(prev_ctx);

        w1.cur_ctx_ = prev_ctx;

#if MADI_ENABLE_STEAL_PROF
        // FIXME: if this thread is stolen
        long t0 = g_prof->current_steal().tmp;
        if (t0 != 0) {
            long t1 = rdtsc();
            g_prof->current_steal().resume = t1 - t0;
            g_prof->next_steal();

            g_prof->current_steal().tmp = 0;

            // call a function registered by at_thread_resuming
            madi::proc().call_thread_resuming();

            MADI_DPUTSR1("resume done: %d", g_prof->steals_idx);
        }
#endif

        if (rank0 == rank1) {
            // not stolen
            logger::checkpoint<logger::kind::WORKER_RESUME_PARENT>();
        } else {
            // stolen
            logger::checkpoint<logger::kind::WORKER_RESUME_STOLEN>();
        }
    }

    inline void worker::exit()
    {
        MADI_NOT_REACHED;
    }

    inline void worker::collect_suspended_freed_remotely()
    {
        logger::begin_data bd = logger::begin_event<logger::kind::COLLECT_SUSPENDED>();
        uint64_t count = 0;

        saved_context* sctx = suspended_threads_;
        while (sctx) {
            if (sctx->header.is_freed == freed_val_) {
                saved_context* sctx_freed = sctx;
                sctx = sctx->header.next;
                free_suspended_local(sctx_freed);
            } else {
                sctx = sctx->header.next;
            }
            count++;
        }

        logger::end_event<logger::kind::COLLECT_SUSPENDED>(bd, count);
    }

    inline saved_context* worker::alloc_suspended(size_t size)
    {
        uth_comm& c = madi::proc().com();
        saved_context* ret;

        ret = (saved_context *)c.malloc_shared_local(size);
        if (ret == NULL) {
            collect_suspended_freed_remotely();
            ret = (saved_context *)c.malloc_shared_local(size);
        }

        if (ret == NULL) {
            madi::die("Allocation failed because of too small initial memory allocation size");
        }

        ret->header.is_freed = 0;
        ret->header.prev = NULL;
        ret->header.next = suspended_threads_;
        if (suspended_threads_) {
            suspended_threads_->header.prev = ret;
        }
        suspended_threads_ = ret;

        return ret;
    }

    inline void worker::free_suspended_local(saved_context* sctx)
    {
        uth_comm& c = madi::proc().com();

        saved_context* sctx_prev = sctx->header.prev;
        saved_context* sctx_next = sctx->header.next;
        if (sctx_prev) {
            sctx_prev->header.next = sctx_next;
        } else {
            suspended_threads_ = sctx_next;
        }
        if (sctx_next) {
            sctx_next->header.prev = sctx_prev;
        }

        c.free_shared_local((void *)sctx);
    }

    inline void worker::free_suspended_remote(saved_context* sctx, pid_t target)
    {
        uth_comm& c = madi::proc().com();

        c.put_nbi(&sctx->header.is_freed, &freed_val_, sizeof(freed_val_), target);
    }

    template <class F, class... Args>
    void worker_do_suspend(context *ctx_ptr, void *f_ptr, void *arg_ptr)
    {
        F f = (F)f_ptr;
        std::tuple<saved_context *, Args...> arg =
            *(std::tuple<saved_context *, Args...> *)arg_ptr;
        worker& w0 = madi::current_worker();

        saved_context *sctx = NULL;

        // pack the current thread (stack)
        MADI_THREAD_PACK(ctx_ptr, w0, &sctx);

        if (w0.is_main_task_) {
            w0.main_sctx_ = sctx;
        } else {
            MADI_CONTEXT_ASSERT(ctx_ptr);
            MADI_SCONTEXT_ASSERT(sctx);
        }

        // update the first argument of f from NULL to sctx
        std::get<0>(arg) = sctx;

        w0.cur_ctx_ = NULL;

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

        context *prev_ctx = w0.cur_ctx_;

        if (!w0.is_main_task_)
            MADI_CONTEXT_ASSERT_WITHOUT_PARENT(prev_ctx);

        void (*fp)(context *, void *, void *) = worker_do_suspend<F, Args...>;

        saved_context *sctx = NULL;
        std::tuple<saved_context *, Args...> arg(sctx, args...);

        // save current state
        MADI_SAVE_CONTEXT_WITH_CALL(prev_ctx, fp, (void *)f, (void *)&arg);

        if (!w0.is_main_task_)
            MADI_CONTEXT_ASSERT_WITHOUT_PARENT(prev_ctx);

        madi::current_worker().cur_ctx_ = prev_ctx;

        // call a function registered by at_thread_resuming
        madi::proc().call_thread_resuming();
    }

    inline void worker::resume(saved_context *next_sctx)
    {
        this->is_main_task_ = next_sctx->is_main_task;

        uint8_t *next_stack_top = (uint8_t *)next_sctx->sp - 128;
        MADI_EXECUTE_ON_STACK(madi_worker_do_resume_saved_context,
                              next_sctx, NULL, NULL, NULL,
                              next_stack_top);
    }

    inline void worker::resume_remote_suspended(suspended_entry se)
    {
        uint8_t *next_stack_top = se.stack_top - 128;
        MADI_EXECUTE_ON_STACK(madi_worker_do_resume_remote_suspended,
                              se.base, se.size, se.stack_top, se.pid,
                              next_stack_top);
    }

    inline void worker::resume_main_task()
    {
        MADI_ASSERT(!is_main_task_ && main_sctx_ != NULL);

        MADI_DPUTS1("a main task is resuming");

        resume(main_sctx_);
    }

}

#endif
