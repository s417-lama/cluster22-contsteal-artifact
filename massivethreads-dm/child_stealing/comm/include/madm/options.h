#ifndef MADI_COMM_OPTIONS_H
#define MADI_COMM_OPTIONS_H

#include <cstddef>
#include <cstdio>

namespace madi {
namespace comm {

    struct options {
        size_t page_size;               // OS page size
        size_t n_procs_per_node;        // # of processes within a node
        size_t server_mod;              // modulo number that determines
                                        // communication server processes
        size_t n_max_sends;             // a parameter for active messaging
        size_t gasnet_poll_thread;      // spawn a poll thread 
                                        //   for GASNet active messaging or not
        size_t gasnet_segment_size;     // RDMA segment size passed to GASNet
        int debug_level;                // debug level (enabled only if
                                        //   configured with debug option)
    };

    extern options options;

    void options_initialize();
    void options_finalize();
    void options_print(FILE *f);
}
}

#endif
