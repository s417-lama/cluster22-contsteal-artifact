#include <cstdio>
#include <cstdlib>
#include <memory>
#include <madm.h>
#include "process-inl.h"
#include "iso_stack_ex/taskq.h"

#include <sys/time.h>
#include <cassert>

using namespace madi;

void real_main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s N\n", argv[0]);
        madi::exit(1);
    }
    long n = atol(argv[1]);

    madm::pid_t me = madm::pid();
    size_t n_procs = madm::n_procs();

    uth_comm& c = madi::proc().com();

    size_t n_entries = 1024;
    size_t entries_size = sizeof(taskq_entry) * n_entries;

    taskque ** taskq_array =
        (taskque **)c.malloc_shared(sizeof(taskque));
    taskq_entry ** taskq_entries_array =
        (taskq_entry **)c.malloc_shared(entries_size);

    taskque *taskq = new (taskq_array[me]) taskque();
    taskq->initialize(taskq_entries_array[me], n_entries);


    madm::barrier();

    int count = 0;
    if (me == 0) {
        for (long i = 0; i < n; i++) {
            taskq_entry entry;
            entry.frame_base = (uint8_t *)(i);
            entry.frame_size = i + 1;
            entry.ctx = (context *)(i + 2);

            taskq->push(entry);

            taskq_entry popped;
            bool success = taskq->pop(&popped);

            if (success) {
                assert(entry.frame_base == popped.frame_base);
                assert(entry.frame_size == popped.frame_size);
                assert(entry.ctx == popped.ctx);
            } else {
                count += 1;

                MADI_DPUTS("entry.frame_base = %ld, count = %d",
                           (long)entry.frame_base, count);
                MADI_DPUTS("entry.frame_size = %ld",
                           (long)entry.frame_size);
                MADI_DPUTS("entry.ctx        = %ld",
                           (long)entry.ctx);
            }
        }
        MADI_DPUTS("DONE");
    } else {
        madm::pid_t target = 0;
        taskque *q = taskq_array[target];

        while (true) {
            bool success = q->steal_trylock(c, target);

            if (success) {
                taskq_entry entry;
                success = q->steal(c, target, taskq_entries_array[target],
                                   &entry);
                q->steal_unlock(c, target);

                if (success) {
                    count += 1;

                    MADI_DPUTS("entry.frame_base = %ld, count = %d",
                               (long)entry.frame_base, count);
                    MADI_DPUTS("entry.frame_size = %ld",
                               (long)entry.frame_size);
                    MADI_DPUTS("entry.ctx        = %ld",
                               (long)entry.ctx);

                }
            }
        }
    }
}

int main(int argc, char **argv) {
    madm::env env(argc, argv);

    madi::proc().start(real_main, argc, argv);

    return 0;
}
