#ifndef MADI_PROCESS_H
#define MADI_PROCESS_H

#include <cstdio>
#include "uni/taskq.h"
#include "uni/worker.h"
#include "uth_comm.h"
#include "iso_space.h"
#include "misc.h"

#include <vector>

namespace madi {

    class process : noncopyable {
    public:
        bool initialized_;
        uth_comm comm_;
        iso_space ispace_;
        void (*user_poll_)();
        FILE *debug_out_;
        void (*at_parent_is_stolen_)();
        void (*at_thread_resuming_)();

        //std::vector<worker> workers_;
        worker workers_[1];

    public:
        process();
        ~process();
      
        bool initialize(int& argc, char **& argv);
        void finalize();

        template <class F, class... Args>
        void start(F f, int& argc, char **& argv, Args... args);
        
        bool initialized();
        uth_comm& com();
        iso_space& ispace();
        FILE *debug_out();
        worker& worker_from_id(size_t id);

        void set_user_poll(void (*poll)());
        void call_user_poll();

        void set_parent_is_stolen(void (*at_stolen)());
        void set_thread_resuming(void (*at_resuming)());
        void call_parent_is_stolen();
        void call_thread_resuming();
    };
    
}

#endif

