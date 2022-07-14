#ifndef MADM_UTH_CXX_DECLS_H
#define MADM_UTH_CXX_DECLS_H

#include <cstddef>
#include <cstdio>

namespace madm {
namespace uth {

    typedef size_t pid_t;

    template <class F, class... Args>
    void start(F f, int& argc, char **& argv, Args... args);

    pid_t get_pid();
    size_t get_n_procs();

    void barrier();

    void poll();

    void set_user_poll(void (*poll)());

    long tick();
    double time();

    void print_options(FILE *f);
}
}

#include "uth/thread.h"

#endif
