
#include "uth.h"
#include "process.h"
#include "madi-inl.h"
#include "future-inl.h"
#include "uth_options.h"

namespace madm {
namespace uth {

    madm::uth::pid_t get_pid() { return madi::proc().com().get_pid(); }
    size_t get_n_procs() { return madi::proc().com().get_n_procs(); }

    void barrier() { madi::barrier(); }

    void discard_all_futures()
    {
        madi::worker& w = madi::current_worker();
        w.fpool().discard_all_futures();
    }

    void set_user_poll(void (*poll)())
    {
        madi::proc().set_user_poll(poll);
    }

    void print_options(FILE *f)
    {
        madi::comm::print_options(f);
        madi::uth_options_print(f);
    }

}
}
