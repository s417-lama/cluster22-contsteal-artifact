#include "process.h"

#include <cstdio>
#include "madm_logger.h"
#include "uth_options.h"
#include "iso_space-inl.h"
#include "madi-inl.h"
#include "debug.h"
#include "uth.h"

namespace madi {

    void do_nothing() {}

    process::process() :
        initialized_(false),
        comm_(), ispace_(),
        user_poll_(do_nothing),
        debug_out_(stderr),
        at_parent_is_stolen_(do_nothing),
        at_thread_resuming_(do_nothing),
        workers_()
    {
    }

    process::~process()
    {
    }

    bool process_do_initialize(process& self, int& argc, char **& argv)
    {
        self.ispace_.initialize(self.comm_);

        MADI_DPUTS2("iso-address space initialized");

        self.workers_[0].initialize(self.comm_);

        MADI_DPUTS2("worker initialized");

        self.initialized_ = true;

        MADI_DPUTS2("MassiveThreads/DM system initialized");

        native_barrier();

        return self.initialized_;
    }

    void process_do_finalize(process& self)
    {
        native_barrier();

        MADI_DPUTS2("start finalization");

        self.initialized_ = false;

        self.workers_[0].finalize(self.comm_);

        MADI_DPUTS2("worker finalized");

        self.ispace_.finalize(self.comm_);

        MADI_DPUTS2("iso-address space finalized");
    }

    bool process::initialize(int& argc, char **& argv)
    {
        debug_out_ = stderr;

        uth_options_initialize();

        g_prof = new prof();

        bool result = comm_.initialize(argc, argv);

        if (!result)
            return false;

        MADI_DPUTS2("communication layer initialized");

        // FIXME: return do_initialize(*this, argc, argv);

        return true;
    }

    void process::finalize()
    {
        g_prof->output();
        delete g_prof;

        // FIXME: do_finalize(*this);

        comm_.finalize();

        MADI_DPUTS2("communication layer finalized");

        logger::clear();

        uth_options_finalize();

        MADI_DPUTS2("MassiveThreads/DM system finalized");
    }

    void process::set_user_poll(void (*poll)())
    {
        user_poll_ = poll;
    }

    void process::set_parent_is_stolen(void (*at_stolen)())
    {
        at_parent_is_stolen_ = at_stolen;
    }

    void process::set_thread_resuming(void (*at_resuming)())
    {
        at_thread_resuming_ = at_resuming;
    }

}
