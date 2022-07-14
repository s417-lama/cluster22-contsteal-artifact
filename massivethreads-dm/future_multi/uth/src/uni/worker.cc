#include "uni/worker.h"
#include "madi.h"
#include "uni/taskq.h"
#include "uth_options.h"
#include "debug.h"
#include "madm_logger.h"

#include "madi-inl.h"
#include "process-inl.h"
#include "iso_space-inl.h"
#include "uth_comm-inl.h"
#include "future-inl.h"
#include "uni/worker-inl.h"

#include <unistd.h>


using namespace madi;
typedef madi::comm::aminfo aminfo;

extern "C" {
    void madi_worker_do_resume_saved_context(void *p0, void *p1, void *p2,
                                             void *p3);
    void madi_worker_do_resume_remote_suspended(void *p0, void *p1, void *p2,
                                                void *p3);
    void madi_resume_context(context *ctx);
}

namespace madi {

struct steal_req {
    steal_req *req_ptr;
    steal_rep *result_rep;
    bool done;
};

struct steal_rep {
    steal_req *req_ptr;
    bool success;
    taskq_entry entry;
    char frames[1];
};


worker::worker() :
    wls_(NULL),
    cur_ctx_(NULL),
    is_main_task_(false),
    taskq_(NULL), taskq_array_(NULL), taskq_entries_array_(NULL),
    fpool_(),
    main_sctx_(NULL)
{
}

worker::~worker()
{
}

worker::worker(const worker& sched) :
    wls_(NULL),
    cur_ctx_(NULL),
    is_main_task_(false),
    taskq_(), taskq_array_(NULL), taskq_entries_array_(NULL),
    fpool_(),
    main_sctx_(NULL)
{
}

const worker& worker::operator=(const worker& sched)
{
    MADI_NOT_REACHED;
}

void worker::initialize(uth_comm& c)
{
    pid_t me = c.get_pid();

    size_t n_entries = madi::uth_options.taskq_capacity;
    size_t entries_size = sizeof(taskq_entry) * n_entries;

    taskque ** taskq_array =
        (taskque **)c.malloc_shared(sizeof(taskque));

    MADI_ASSERT(taskq_array[me] != NULL);

    taskq_entry ** taskq_entries_array =
        (taskq_entry **)c.malloc_shared(entries_size);

    MADI_ASSERT(taskq_entries_array[me] != NULL);

    taskque *taskq_buf =
        (taskque *)c.malloc_shared_local(sizeof(taskque));

    MADI_ASSERT(taskq_buf != NULL);

    taskq_entry *taskq_entry_buf =
        (taskq_entry *)c.malloc_shared_local(sizeof(taskq_entry));

    MADI_ASSERT(taskq_entry_buf != NULL);

    taskque *taskq = new (taskq_array[me]) taskque();
    taskq->initialize(c, taskq_entries_array[me], n_entries);

    taskq_ = taskq;
    taskq_array_ = taskq_array;
    taskq_entries_array_ = taskq_entries_array;
    taskq_buf_ = taskq_buf;
    taskq_entry_buf_ = taskq_entry_buf;

    size_t future_buf_size = get_env("MADM_FUTURE_POOL_BUF_SIZE", 128 * 1024); // FIXME: does not work with 4MB
    fpool_.initialize(c, future_buf_size);
}

void worker::finalize(uth_comm& c)
{
    fpool_.finalize(c);
    taskq_->finalize(c);

    c.free_shared((void **)taskq_array_);
    c.free_shared((void **)taskq_entries_array_);
    c.free_shared_local((void *)taskq_buf_);
    c.free_shared_local((void *)taskq_entry_buf_);

    taskq_ = NULL;
    taskq_array_ = NULL;
    taskq_entries_array_ = NULL;
    taskq_buf_ = NULL;
    taskq_entry_buf_ = NULL;
}

void worker::do_scheduler_work()
{
    uth_comm& c = madi::proc().com();

    // call a user-specified polling function
    madi::proc().call_user_poll();

    // call a polling function for communication progress
    MADI_UTH_COMM_POLL();

    // work stealing
    bool success = steal();

    // do nothing (stolen function is resumed at the steal() function)
}

}

extern "C" {

void madi_resume_context(context *ctx)
{
    //MADI_CONTEXT_ASSERT(ctx);

    MADI_RESUME_CONTEXT(ctx);
}

__attribute__((noinline))
void madi_worker_do_resume_saved_context(void *p0, void *p1, void *p2, void *p3)
{
    saved_context *sctx = (saved_context *)p0;

    context *ctx = sctx->ctx;

    void *rsp;
    MADI_GET_CURRENT_SP(&rsp);
    MADI_DPUTS2("current rsp      = %p", rsp);

    MADI_SCONTEXT_PRINT(2, sctx);
    MADI_SCONTEXT_ASSERT(sctx);

    uint8_t *frame_base = sctx->stack_top;
    size_t frame_size = sctx->stack_size;

    memcpy(frame_base, sctx->partial_stack, frame_size);

    MADI_CONTEXT_PRINT(2, ctx);
    if (!sctx->is_main_task)
        MADI_CONTEXT_ASSERT(ctx);

    MADI_ASSERT(sctx->ip == ctx->instr_ptr());
    MADI_ASSERT(sctx->sp == ctx->stack_ptr());

    worker& w = madi::current_worker();

    w.free_suspended_local(sctx);

    MADI_DPUTSR2("resuming  [%p, %p) (size = %zu) (waiting)",
                 frame_base, frame_base + frame_size, frame_size);

    madi_resume_context(ctx);
}

__attribute__((noinline))
void madi_worker_do_resume_remote_suspended(void *p0, void *p1, void *p2, void *p3)
{
    saved_context* remote_sctx = (saved_context*)p0;
    size_t size = (size_t)p1;
    uint8_t* stack_top = (uint8_t*)p2;
    uth_pid_t target = (uth_pid_t)p3;

    uth_comm& c = madi::proc().com();

    uint8_t* base = (uint8_t*)remote_sctx;
    size_t stack_offset = offsetof(saved_context, partial_stack);
    size_t frame_size = size - stack_offset;

    c.get(stack_top, base + stack_offset, frame_size, target);

    worker& w = madi::current_worker();

    w.free_suspended_remote(remote_sctx, target);

    MADI_DPUTSR2("resuming  [%p, %p) (size = %zu) (waiting)",
                 stack_top, stack_top + frame_size, frame_size);

    context* ctx = (context*)stack_top;
    madi_resume_context(ctx);
}

__attribute__((noinline))
void madi_worker_do_resume_remote_context_1(uth_comm& c,
                                            uth_pid_t victim,
                                            taskque *taskq,
                                            taskq_entry *entry)
{
    MADI_UNUSED uint8_t *frame_base = (uint8_t *)entry->frame_base;
    MADI_UNUSED size_t frame_size = entry->frame_size;

    context *ctx = (context*)entry->frame_base;

    MADI_TENTRY_PRINT(2, entry);
    MADI_CONTEXT_PRINT(2, ctx);

    taskq->unlock(c, victim);

    MADI_DPUTSR1("resuming  [%p, %p) (size = %zu) (stolen)",
                 frame_base, frame_base + frame_size, frame_size);

    // resume the context of the stolen thread
    madi_resume_context(ctx);
}

__attribute__((noinline))
void madi_worker_do_resume_remote_context(void *p0, void *p1, void *p2,
                                          void *p3)
{
    // data pointed from the parameter pointers may be corrupted
    // by stack copy, so we have to copy it to the current stack frame.

    std::tuple<taskq_entry *, uth_pid_t, taskque *>& arg =
        *(std::tuple<taskq_entry *, uth_pid_t, taskque *> *)p0;

    taskq_entry entry = *std::get<0>(arg);
    uth_pid_t victim = std::get<1>(arg);
    taskque *taskq = std::get<2>(arg);

    iso_space& ispace = madi::proc().ispace();
    uth_comm& c = madi::proc().com();
    uth_pid_t me = c.get_pid();

    uint8_t *frame_base = (uint8_t *)entry.frame_base;
    size_t frame_size = entry.frame_size;

    MADI_DPUTSR1("RDMA region [%p, %p) (size = %zu)",
                 frame_base, (uint8_t *)frame_base + frame_size,
                 frame_size);
    MADI_TENTRY_PRINT(2, &entry);

    // alignment for RDMA operations
    frame_base = (uint8_t *)((uintptr_t)frame_base & ~0x3);
    frame_size = (frame_size + 4) / 4 * 4;

    MADI_DEBUG2({
        uint8_t *sp;
        MADI_GET_CURRENT_SP(&sp);

        MADI_DPUTS("sp = %p", sp);

        MADI_ASSERT(sp <= frame_base);
    });

    c.reg_get(frame_base, frame_base, frame_size, victim);

    madi_worker_do_resume_remote_context_1(c, victim, taskq, &entry);
}

__attribute__((noinline))
void madi_worker_do_resume_remote_context_by_messages_1(steal_rep *rep)
{
    taskq_entry *entry = &rep->entry;
    MADI_UNUSED uint8_t *frame_base = (uint8_t *)entry->frame_base;
    MADI_UNUSED size_t frame_size = entry->frame_size;

    context *ctx = (context*)entry->frame_base;

    MADI_TENTRY_PRINT(2, entry);
    MADI_CONTEXT_PRINT(2, ctx);

    MADI_DPUTSR1("resuming  [%p, %p) (size = %zu) (stolen)",
                 frame_base, frame_base + frame_size, frame_size);

    free(rep); // allocated at reply_steal

    madi_resume_context(ctx);
}

__attribute__((noinline))
void madi_worker_do_resume_remote_context_by_messages(
    void *p0, void *p1, void *p2, void *p3)
{
    // data pointed from the parameter pointers may be corrupted
    // by stack copy, so we have to copy it to the current stack frame.

    steal_rep *rep = (steal_rep *)p0;
    taskq_entry& entry = rep->entry;
    char *frames       = rep->frames;

    uth_comm& c = madi::proc().com();

    uint8_t *frame_base = (uint8_t *)entry.frame_base;
    size_t frame_size = entry.frame_size;

    MADI_DPUTSR1("RDMA region [%p, %p) (size = %zu)",
                 frame_base, (uint8_t *)frame_base + frame_size,
                 frame_size);
    MADI_TENTRY_PRINT(2, &entry);

    // frame copy
    memcpy(frame_base, frames, frame_size);

    // resume
    madi_worker_do_resume_remote_context_by_messages_1(rep);
}

}

namespace madi {

static size_t select_victim_randomly(uth_comm& c)
{
    uth_pid_t me = c.get_pid();
    size_t n_procs = c.get_n_procs();

    uth_pid_t pid;
    do {
        pid = (uth_pid_t)random_int((int)n_procs);
        MADI_CHECK((size_t)pid != n_procs);
    } while (pid == me);

    return pid;
}

static size_t select_victim(uth_comm& c)
{
    return select_victim_randomly(c);
}

bool worker::steal_with_lock(taskq_entry *entry,
                             uth_pid_t *victim,
                             taskque **taskq_ptr)
{
    uth_comm& c = madi::proc().com();

    size_t target = select_victim(c);
    *victim = target;

    taskq_entry *entries = taskq_entries_array_[target];
    taskque *taskq = taskq_array_[target];

    if (uth_options.aborting_steal) {
        bool do_abort = taskq->empty(c, target, taskq_buf_);

        if (do_abort) {
            return false;
        }
    }

    bool success;
    success = taskq->trylock(c, target);

    if (!success) {
        MADI_DPUTSR1("steal lock failed");
        return false;
    }

    success = taskq->steal(c, target, entries, entry, taskq_buf_);

    if (!success) {
        MADI_DPUTSR1("steal task empty");
        taskq->unlock(c, target);

        return false;
    }

    *taskq_ptr = taskq;  // for unlock when task stack is transfered
    return true;
}

void resume_remote_evacuated_context(saved_context *sctx,
                                     suspended_entry *se)
{
    worker& w = madi::current_worker();

    w.is_main_task_ = false;

    w.resume_remote_suspended(*se);
}

void resume_remote_context(saved_context *sctx,
                           std::tuple<taskq_entry *, uth_pid_t,
                                      taskque *> *arg)
{
    taskq_entry *entry = std::get<0>(*arg);

    worker& w = madi::current_worker();

    // steal should occur only on the main thread
    MADI_ASSERT(w.is_main_task_);

    w.is_main_task_ = false;

    uint8_t *next_stack_top = (uint8_t *)entry->frame_base;
    MADI_EXECUTE_ON_STACK(madi_worker_do_resume_remote_context,
                          arg, NULL, NULL, NULL,
                          next_stack_top);
}

bool worker::steal_by_rdmas()
{
    uth_pid_t victim;

    taskq_entry& stolen_entry = *taskq_entry_buf_;
    taskque *taskq;

    bool success = steal_with_lock(&stolen_entry, &victim, &taskq);

    if (success) {
        // next_steal() is called when stolen thread resumed.
    } else {
        // do not record when a steal fails.
        //g_prof->next_steal();
    }

    if (success) {
        logger::checkpoint<logger::kind::WORKER_SCHED>();

        // switch to the stolen task
        MADI_DPUTSB2("resuming a stolen task");

        if (stolen_entry.stack_top == 0) {
            // the stack frame is in the uni-address region
            std::tuple<taskq_entry *, uth_pid_t, taskque *>
                arg(&stolen_entry, victim, taskq);

            suspend(resume_remote_context, &arg);
        } else {
            // no need to protect the taskq lock until the stack copy is completed
            // because the stack has already been evacuated from the uni-address region
            uth_comm& c = madi::proc().com();
            taskq->unlock(c, victim);

            suspended_entry se;
            se.base      = stolen_entry.frame_base;
            se.size      = stolen_entry.frame_size;
            se.pid       = stolen_entry.pid;
            se.stack_top = stolen_entry.stack_top;

            suspend(resume_remote_evacuated_context, &se);
        }
    }

    return success;
}

void handle_steal_reply(int tag, int pid, void *p, size_t size, aminfo *info)
{
    steal_rep *rep = reinterpret_cast<steal_rep *>(p);
    steal_req *req = rep->req_ptr;

    if (rep->success) {
        // deallocated at madi_worker_do_resume_remote_context_by_messages_1
        steal_rep *rep1 = (steal_rep *)malloc(size);
        memcpy(rep1, rep, size);

        req->result_rep = rep1;
    } else {
        req->result_rep = NULL;
    }

    req->done = true;
}

void handle_steal_request(int tag, int pid, void *p, size_t size, aminfo *info)
{
    uth_comm& c = madi::proc().com();
    pid_t me = c.get_pid();
    worker& w = madi::current_worker();
    taskque& taskq = w.taskq();

    steal_req *req = reinterpret_cast<steal_req *>(p);

    bool success = taskq.trylock(c, me);

    if (!success) {
        steal_rep rep;
        rep.req_ptr = req->req_ptr;
        rep.success = false;
        c.amreply(uth_comm::AM_STEAL_REP, &rep, sizeof(rep), info);

        MADI_DP((int)success);
        return;
    }

    taskq_entry entry;
    success = taskq.local_steal(&entry);

    if (success) {
        // make msg body
        size_t rep_size = offsetof(steal_rep, frames) + entry.frame_size;
        steal_rep *rep = (steal_rep *)malloc(rep_size);
        rep->req_ptr = req->req_ptr;
        rep->success = true;
        rep->entry   = entry;
        memcpy(rep->frames, entry.frame_base, entry.frame_size);

        taskq.unlock(c, me);

        // reply msg
        c.amreply(uth_comm::AM_STEAL_REP, rep, rep_size, info);

        free(rep);
    } else {
        taskq.unlock(c, me);

        steal_rep rep;
        rep.req_ptr = req->req_ptr;
        rep.success = false;
        c.amreply(uth_comm::AM_STEAL_REP, &rep, sizeof(rep), info);
    }


    MADI_DP((int)success);
}

void resume_remote_context_by_messages(saved_context *sctx,
                                       steal_rep *rep)
{
    worker& w = madi::current_worker();
    taskq_entry *entry = &rep->entry;

    MADI_ASSERT(sctx != NULL);

    /* w.waitq().push_back(sctx); */

    w.is_main_task_ = false;

    uint8_t *next_stack_top = (uint8_t *)entry->frame_base;
    MADI_EXECUTE_ON_STACK(madi_worker_do_resume_remote_context_by_messages,
                          sctx, rep, NULL, NULL,
                          next_stack_top);
}

bool worker::steal_by_messages()
{
    uth_comm& c = madi::proc().com();
  
    size_t target = select_victim(c);

    steal_req req;
    req.done       = false;
    req.result_rep = NULL;

    steal_req *arg = &req;
    c.amrequest(uth_comm::AM_STEAL_REQ, &arg, sizeof(arg), target);

    while (!req.done)
        MADI_UTH_COMM_POLL();

    bool success = (req.result_rep != NULL);

    if (success) {
        steal_rep *rep = req.result_rep;

        // work stealing succeeds
        suspend(resume_remote_context_by_messages, rep);
    }

    return success;
}

bool worker::steal()
{
#if 1
    return steal_by_rdmas();
#else
    return steal_by_messages();
#endif
}

}
