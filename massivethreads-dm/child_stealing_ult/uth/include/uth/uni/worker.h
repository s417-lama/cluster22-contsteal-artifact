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

    struct task_general {
        virtual void execute() = 0;
    };

    template <typename F, class... Args>
    struct callable_task : task_general {
        F f;
        std::tuple<Args...> arg;
        callable_task(F f_, Args... args) : f(f_), arg(args...) {}
        void execute() { tuple_apply<void>::f<F, Args...>(f, arg); }
    };

    struct free_list_entry {
        struct free_list_entry* next;
    };

    template <class F, class... Args>
    inline void worker_start(void *arg0, void *arg1, void *arg2, void *arg3);
    template <class F, class... Args>
    inline void worker_do_start(context *ctx, void *arg0, void *arg1);

    class worker {
        template <class F, class... Args>
        friend void worker_do_suspend(context *ctx_ptr,
                                      void *f_ptr, void *arg_ptr);
        template <class F, class... Args>
        friend void worker_start(void *arg0, void *arg1, void *arg2,
                                 void *arg3, Args... args);
        template <class F, class... Args>
        friend void worker_do_start(context *ctx, void *arg0, void *arg1);
        friend void do_start_new_task(task_general* task, void* stack, size_t stolen);
        friend void start_new_task(context *ctx, task_general* task, size_t stolen);

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

        context *main_ctx_;
        std::deque<context *> waitq_;

        free_list_entry* free_list_;
        size_t stack_size_;

        bool done_;

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

        void resume(context *next_ctx);

        void do_scheduler_work();

        future_pool& fpool() { return fpool_; }
        taskque& taskq() { return *taskq_; }
        std::deque<context *>& waitq() { return waitq_; }

        size_t max_stack_usage() const { return max_stack_usage_; }

        template <class T> void set_worker_local(T *value)
        { wls_= reinterpret_cast<void *>(value); }

        template <class T> T * get_worker_local()
        { return reinterpret_cast<T *>(wls_); }

    private:
        bool steal_with_lock(taskq_entry *entry,
                             uth_pid_t *victim,
                             task_general **task);
        bool steal(task_general** task);
    };

}

#endif
