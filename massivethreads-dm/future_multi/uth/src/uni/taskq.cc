#include "taskq.h"
#include "debug.h"
#include "uth_comm.h"

using namespace madi;

global_taskque::global_taskque() :
    top_(0), base_(0),
    n_entries_(0), entries_(NULL)
{
}

global_taskque::~global_taskque()
{
}

void global_taskque::initialize(uth_comm& c, taskq_entry *entries,
                                size_t n_entries)
{
    MADI_CHECK(n_entries <= INT_MAX);
    MADI_CHECK(entries != NULL);

    base_ = 0;
    top_ = 0;
    n_entries_ = (int)n_entries;
    entries_ = entries;
    local_empty_ = true;

    c.lock_init(&lock_);
}

void global_taskque::finalize(uth_comm& c)
{
    top_ = 0;
    base_ = 0;
    n_entries_ = 0;
    entries_ = NULL;
}
