#ifndef MADI_ISO_STACK_EX_WORKER_H
#define MADI_ISO_STACK_EX_WORKER_H

#include "../madi.h"
#include "taskq.h"
#include "context.h"
#include "../future.h"
#include "../debug.h"
#include <deque>
#include <tuple>

#ifndef MADI_ENABLE_STEAL_PROF
#define MADI_ENABLE_STEAL_PROF 0
#endif

namespace madi {

    template <class F, class... Args>
    struct start_params;

    struct steal_rep;

    template <class F, class... Args>
    inline void worker_start(void *arg0, void *arg1, void *arg2, void *arg3);
    template <class F, class... Args>
    inline void worker_do_start(context *ctx, void *arg0, void *arg1);

    class worker {
        template <class F, class... Args>
        friend void worker_do_fork(context *ctx_ptr,
                                   void *f_ptr, void *arg_ptr);

        template <class F, class... Args>
        friend void worker_do_suspend(context *ctx_ptr,
                                      void *f_ptr, void *arg_ptr);

        template <class F, class... Args>
        friend void worker_start(void *arg0, void *arg1, void *arg2,
                                 void *arg3, Args... args);
        template <class F, class... Args>
        friend void worker_do_start(context *ctx, void *arg0, void *arg1);

        friend void madi_worker_do_resume_remote(void *p0, void *p1,
                                                 void *p2, void *p3);
        friend void resume_remote_context(saved_context *sctx,
                                          std::tuple<taskq_entry *,
                                          uth_pid_t, taskque *, logger::begin_data> *arg);
        friend void resume_remote_context_by_messages(saved_context *sctx,
                                                      steal_rep *rep);

        int freed_val_ = 1;

        void *wls_;
        bool is_main_task_;
        size_t max_stack_usage_;
        uint8_t *stack_bottom_;

        context* cur_ctx_;

        taskque *taskq_;
        taskque **taskq_array_;
        taskq_entry **taskq_entries_array_;
        taskque *taskq_buf_;
        taskq_entry *taskq_entry_buf_;

        future_pool fpool_;

        saved_context* main_sctx_;

        saved_context* suspended_threads_ = NULL;

    public:
        worker();
        ~worker();

        worker(const worker& sched);
        const worker& operator=(const worker& sched);

        void initialize(uth_comm& c);
        void finalize(uth_comm& c);

        template <class F, class... Args>
        void start(F f, int argc, char **argv, Args... args);

        void notify_done();

        template <class F, class... Args>
        void fork(F f, Args... args);

        void exit();

        template <class F, class... Args>
        void suspend(F f, Args... args);

        void resume(saved_context *next_sctx);

        void resume_remote_suspended(suspended_entry se);

        void do_scheduler_work();

        void resume_main_task();

        future_pool& fpool() { return fpool_; }
        taskque& taskq() { return *taskq_; }

        size_t max_stack_usage() const { return max_stack_usage_; }

        template <class T> void set_worker_local(T *value)
        { wls_= reinterpret_cast<void *>(value); }

        template <class T> T * get_worker_local()
        { return reinterpret_cast<T *>(wls_); }

        bool is_main_task() { return is_main_task_; }

        saved_context* alloc_suspended(size_t size);
        void free_suspended_local(saved_context* sctx);
        void free_suspended_remote(saved_context* sctx, pid_t target);

    private:
        static void do_resume(worker& w, const taskq_entry& entry,
                              uth_pid_t victim);
        bool steal_with_lock(taskq_entry *entry,
                             uth_pid_t *victim,
                             taskque **taskq);
        bool steal();
        bool steal_by_rdmas();
        bool steal_by_messages();

        void collect_suspended_freed_remotely();
    };

}

#endif
