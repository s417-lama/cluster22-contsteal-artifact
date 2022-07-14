#include "gtest/gtest.h"
#include <cstdio>

int g_argc = 0;
char **g_argv = NULL;

GTEST_API_ int main(int argc, char **argv) {
    printf("Running main() from gtest_main.cc\n");

    g_argc = argc;
    g_argv = argv;

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

