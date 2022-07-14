
#include "madm_comm.h"
#include <gtest/gtest.h>

extern int g_argc;
extern char ** g_argv;

TEST(comm, simple)
{
    bool success = madi::comm::initialize(g_argc, g_argv);

    ASSERT_TRUE(success);

    madi::comm::finalize();
}

