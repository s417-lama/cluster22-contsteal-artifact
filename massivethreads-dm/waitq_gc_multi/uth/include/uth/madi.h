#ifndef MADI_H
#define MADI_H

#include <cstdio>
#include "misc.h"

namespace madi {

    class process;
    class worker;
    class iso_space;

    typedef size_t uth_pid_t;

    bool initialized();
    void exit(int exitcode) MADI_NORETURN;

    void barrier();
    void native_barrier();

    process& proc();
    worker& current_worker();

    FILE *debug_out();

    iso_space& get_iso_space();

    template <class T>
    void set_worker_local(T *value);
    template <class T>
    T * get_worker_local();

    void at_thread_exit(void (*func)());
    void at_parent_is_stolen(void (*func)());
    void at_thread_resuming(void (*func)());
}

#endif
