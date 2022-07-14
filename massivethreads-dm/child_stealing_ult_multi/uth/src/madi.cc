#include "madi-inl.h"
#include "process-inl.h"
#include "uni/worker-inl.h"

namespace madi {

    process madi_process;
    prof *g_prof = NULL;

    void exit(int exitcode) {
        madi::proc().com().exit(exitcode);
    }

    FILE *debug_out() {
        return madi::proc().debug_out();
    }

    iso_space& get_iso_space() {
        return madi::proc().ispace();
    }

    void barrier()
    {
        logger::checkpoint<logger::kind::WORKER_BUSY>();

        uth_comm& c = madi::proc().com();
        worker& w = madi::current_worker();

        while (!c.barrier_try())
            w.do_scheduler_work();

        // update max stack usage
        g_prof->max_stack_usage = w.max_stack_usage();

        logger::checkpoint<logger::kind::WORKER_SCHED>();
    }

    void native_barrier()
    {
        uth_comm& c = madi::proc().com();
        c.barrier();
    }

    void at_parent_is_stolen(void (*func)())
    {
        madi::proc().set_parent_is_stolen(func);
    }

    void at_thread_resuming(void (*func)())
    {
        madi::proc().set_thread_resuming(func);
    }

}

