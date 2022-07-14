#include "uth_options.h"
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

namespace madi {

    // default values of configuration variables
    struct uth_options uth_options = {
        1024 * 1024,        // stack_size
        1,                  // stack_overflow_detection
        1024,               // taskq_capacity
        8192,               // page_size
        0,                  // profile
        0,                  // steal_log
        1,                  // aborting_steal
    };

    template <class T>
    void set_option(const char *name, T *value);

    template <>
    void set_option<int>(const char *name, int *value) {
        char *s = getenv(name);
        if (s != NULL)
            *value = atoi(s);
    }

    template <>
    void set_option<size_t>(const char *name, size_t *value) {
        char *s = getenv(name);

        if (s != NULL) {
            *value = static_cast<size_t>(atol(s));
        }
    }

    void uth_options_initialize()
    {
        set_option("MADM_STACK_SIZE", &uth_options.stack_size);
        set_option("MADM_STACK_DETECT", &uth_options.stack_overflow_detection);
        set_option("MADM_TASKQ_CAPACITY", &uth_options.taskq_capacity);
        set_option("MADM_PROFILE", &uth_options.profile);
        set_option("MADM_STEAL_LOG", &uth_options.steal_log);
        set_option("MADM_ABORTING_STEAL", &uth_options.aborting_steal);

        long page_size = sysconf(_SC_PAGE_SIZE);
        uth_options.page_size = static_cast<size_t>(page_size);
    }

    void uth_options_finalize()
    {
    }

    void uth_options_print(FILE *f)
    {
        fprintf(f,
                "MADM_STACK_SIZE = %zu"
                ", MADM_TASKQ_CAPACITY = %zu"
                ", MADM_PROFILE = %d"
                ", MADM_STEAL_LOG = %d"
                ", MADM_ABORTING_STEAL = %d"
                "\n",
                uth_options.stack_size,
                uth_options.taskq_capacity,
                uth_options.profile,
                uth_options.steal_log,
                uth_options.aborting_steal);
    }
}
