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

extern "C" {
    void madi_worker_do_resume_saved_context(void *p0, void *p1, void *p2,
                                             void *p3);
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
    waitq_(),
    free_list_(NULL),
    main_ctx_(NULL),
    done_(false)
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
    waitq_(),
    free_list_(NULL),
    main_ctx_(NULL), 
    done_(false)
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

    MADI_ASSERT(waitq_.size() == 0);
    waitq_.clear();

    free_list_ = NULL;
    stack_size_ = get_env("MADM_STACK_FRAME_SIZE", 16 * 1024);

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

    free_list_entry* fle = free_list_;
    while (fle != NULL) {
        free_list_entry* tmp = fle;
        fle = fle->next;
        free(tmp);
    }
    free_list_ = NULL;
}

void start_new_task(context *ctx, task_general* task, size_t stolen);

// executed on the new stack
void do_start_new_task(task_general* task, void* stack, size_t stolen)
{
    task->execute();

    uth_comm& c = madi::proc().com();
    if (stolen) {
        free(task);
    } else {
        c.free_shared_local(task);
    }

    worker& w = madi::current_worker();

    assert(!w.waitq_.empty());

    free_list_entry* fle = (free_list_entry*)stack;
    fle->next = w.free_list_;
    w.free_list_ = fle;

    // switch to a waiting task
    MADI_DPUTSB2("resuming a waiting task");
    context *ctx = w.waitq_.front();
    w.waitq_.pop_front();
    w.resume(ctx);

    MADI_NOT_REACHED;
}

void start_new_task(context *ctx, task_general* task, size_t stolen)
{
    uth_comm& c = madi::proc().com();
    worker& w = madi::current_worker();

    if (ctx != NULL)
        w.waitq().push_back(ctx);

    void* stack = NULL;
    if (w.free_list_ != NULL) {
        stack = (void*)w.free_list_;
        w.free_list_ = w.free_list_->next;
    } else {
        posix_memalign(&stack, 4096, w.stack_size_);
    }

    MADI_CALL_ON_NEW_STACK(stack, w.stack_size_,
                           do_start_new_task, task, stack, stolen, NULL);

    MADI_NOT_REACHED;
}

void resume_saved_context(context *ctx, context *next_ctx)
{
    worker& w = madi::current_worker();

    if (ctx != NULL)
        w.waitq().push_back(ctx);

    w.resume(next_ctx);

    MADI_NOT_REACHED;
}

void worker::do_scheduler_work()
{
    uth_comm& c = madi::proc().com();

    // call a user-specified polling function
    madi::proc().call_user_poll();

    // call a polling function for communication progress
    MADI_UTH_COMM_POLL();

    taskq_entry *entry = taskq_->pop(c);
    if (entry != NULL) {
        task_general* task = (task_general*)entry->taskobj_base;
        suspend(start_new_task, task, 0);
    } else {
        // work stealing
        task_general* task;
        bool success = steal(&task);

        if (success) {
            suspend(start_new_task, task, 1);
        } else if (!waitq_.empty()) {
            main_ctx_ = NULL;

            // switch to a waiting task
            MADI_DPUTSB2("resuming a waiting task");
            context *ctx = waitq_.front();
            waitq_.pop_front();
            suspend(resume_saved_context, ctx);
        }
    }
}

void worker::notify_done()
{
    done_ = true;
}

}

extern "C" {

__attribute__((noinline))
void madi_worker_do_resume_saved_context(void *p0, void *p1, void *p2, void *p3)
{
    context *ctx = (context *)p0;

    void *rsp;
    MADI_GET_CURRENT_SP(&rsp);
    MADI_DPUTS2("current rsp      = %p", rsp);

    MADI_RESUME_CONTEXT(ctx);
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
                             task_general **task)
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

    if (success) {
        size_t size = entry->taskobj_size;
        task_general* t = (task_general*)malloc(size);

        logger::begin_data bd = logger::begin_event<logger::kind::STEAL_TASK_COPY>();

        c.get_buffered(t, entry->taskobj_base, size, target);

        logger::end_event<logger::kind::STEAL_TASK_COPY>(bd, size);

        taskq->unlock(c, target);

        *task = t;

        return true;
    } else {
        MADI_DPUTSR1("steal task empty");
        taskq->unlock(c, target);

        *task = NULL;

        return false;
    }
}

bool worker::steal(task_general** task)
{
    uth_comm& c = madi::proc().com();
    uth_pid_t victim;

    size_t n_procs = c.get_n_procs();
    if (n_procs == 1) return false;

    taskq_entry& stolen_entry = *taskq_entry_buf_;

    logger::begin_data bd = logger::begin_event<logger::kind::STEAL_SUCCESS>();

    bool success = steal_with_lock(&stolen_entry, &victim, task);

    if (success) {
        logger::end_event<logger::kind::STEAL_SUCCESS>(bd, victim);
    } else {
        logger::end_event<logger::kind::STEAL_FAIL>(bd, victim);
    }

    return success;
}

}
