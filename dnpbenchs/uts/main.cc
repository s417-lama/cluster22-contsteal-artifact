/*
 *         ---- The Unbalanced Tree Search (UTS) Benchmark ----
 *  
 *  Copyright (c) 2010 See AUTHORS file for copyright holders
 *
 *  This file is part of the unbalanced tree search benchmark.  This
 *  project is licensed under the MIT Open Source license.  See the LICENSE
 *  file for copyright and licensing information.
 *
 *  UTS is a collaborative project between researchers at the University of
 *  Maryland, the University of North Carolina at Chapel Hill, and the Ohio
 *  State University.  See AUTHORS file for more information.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <uth.h>
#include <uth-dtbb.h>
#include <madm_comm.h>

namespace uth = madm::uth;

#include "uts.h"

#ifndef UTS_RUN_SEQ
#define UTS_RUN_SEQ 0
#endif

#ifndef UTS_RECURSIVE_FOR
#define UTS_RECURSIVE_FOR 0
#endif

/***********************************************************
 *  UTS Implementation Hooks                               *
 ***********************************************************/

// The name of this implementation
const char * impl_getName(void) {
    return "MassiveThreads/DM Parallel Search";
}

int impl_paramsToStr(char *strBuf, int ind) {
    ind += sprintf(strBuf + ind, "Execution strategy:  %s\n", impl_getName());
    return ind;
}

// Not using UTS command line params, return non-success
int impl_parseParam(char *param, char *value) {
    return 1;
}

void impl_helpMessage(void) {
    printf("   none.\n");
}

void impl_abort(int err) {
    exit(err);
}

/***********************************************************
 * Recursive depth-first implementation                    *
 ***********************************************************/

typedef struct {
  counter_t maxdepth, size, leaves;
} Result;

static Result mergeResult(Result r0, Result r1)
{
    Result r = {
        (r0.maxdepth > r1.maxdepth) ? r0.maxdepth : r1.maxdepth,
        r0.size + r1.size,
        r0.leaves + r1.leaves
    };
    return r;
}

static Node
makeChild(const Node *parent, int childType, int computeGranuarity,
          counter_t idx)
{
    int j;

    Node c = { childType, (int)parent->height + 1, -1, {{0}} };

    for (j = 0; j < computeGranularity; j++) {
        rng_spawn(parent->state.state, c.state.state, (int)idx);
    }

    return c;
}

//-- sequential -------------------------------------------------------------

static Result parTreeSearch_fj_seq(counter_t depth, Node parent);

#if UTS_RECURSIVE_FOR
static Result
doParTreeSearch_fj_seq(counter_t depth, Node parent, int childType,
                       counter_t numChildren, counter_t begin, counter_t end)
{
    if (end - begin == 1) {
        Node child = makeChild(&parent, childType, computeGranularity, begin);
        return parTreeSearch_fj_seq(depth, child);
    } else {
        counter_t center = (begin + end) / 2;

        Result r0 = doParTreeSearch_fj_seq(depth, parent, childType,
                                           numChildren, begin, center);

        Result r1 = doParTreeSearch_fj_seq(depth, parent, childType,
                                           numChildren, center, end);
        return mergeResult(r0, r1);
    }
}
#endif

static Result parTreeSearch_fj_seq(counter_t depth, Node parent) {
    Result result;

    assert(depth == 0 || parent.height > 0);

    counter_t numChildren = uts_numChildren(&parent);
    int childType = uts_childType(&parent);

    // Recurse on the children
    if (numChildren == 0) {
        Result r = { depth, 1, 1 };
        result = r;
    } else {

#if UTS_RECURSIVE_FOR
        result = doParTreeSearch_fj_seq(depth + 1, parent, childType,
                                        numChildren, 0, numChildren);
#else
        Result init = { 0, 0, 0 };
        result = init;
        for (counter_t i = 0; i < numChildren; i++) {
          Node child = makeChild(&parent, childType, computeGranularity, i);
          Result r = parTreeSearch_fj_seq(depth + 1, child);
          result = mergeResult(result, r);
        }
#endif

        result.size += 1;
    }

    return result;
}

//-- parallel ---------------------------------------------------------------

static Result parTreeSearch_fj(counter_t depth, Node parent) {
    Result result;

    assert(depth == 0 || parent.height > 0);

    counter_t numChildren = uts_numChildren(&parent);
    int childType = uts_childType(&parent);

    // Recurse on the children
    if (numChildren == 0) {
        Result r = { depth, 1, 1 };
        result = r;
    } else {
        Result init = { 0, 0, 0 };
        result = uth::dtbb::parallel_reduce(0ULL, numChildren, init,
                    [=] (counter_t i) {
                        Node child = makeChild(&parent, childType,
                                               computeGranularity, i);
                        return parTreeSearch_fj(depth + 1, child);
                    },
                    mergeResult);

        result.size += 1;
    }

    return result;
}

//-- main ---------------------------------------------------------------------

Result uts_fj_run(counter_t depth, Node parent)
{
#if UTS_RUN_SEQ
    uth::thread<Result> f(parTreeSearch_fj_seq, depth, parent);
#else
    uth::thread<Result> f(parTreeSearch_fj, depth, parent);
#endif
    return f.join();
}

static Result
uts_fj_main(uint64_t *walltime)
{
    Result r;
    Node root;
    uth::pid_t me = uth::get_pid();

    if (me == 0) {
        uts_initRoot(&root, type);

        uint64_t t1 = uts_wctime();

        r = uts_fj_run((counter_t)0, root);

        uint64_t t2 = uts_wctime();

        *walltime = t2 - t1;
    }
    uth::barrier();

    return r;
}

void real_main(int argc, char *argv[]) {
    counter_t nNodes = 0;
    counter_t nLeaves = 0;
    counter_t maxTreeDepth = 0;
    uint64_t  walltime = 0;

    uts_parseParams(argc, argv);

    size_t n_procs = uth::get_n_procs();
    uth::pid_t me = uth::get_pid();

    if (me == 0) {
        setlocale(LC_NUMERIC, "en_US.UTF-8");
        printf("=============================================================\n"
               "[uts]\n"
               "# of processes:                %ld\n"
               "-------------------------------------------------------------\n",
               n_procs);

        if (type == GEO) {
            printf("t (Tree type):                 Geometric (%d)\n"
                   "r (Seed):                      %d\n"
                   "b (Branching factor):          %f\n"
                   "a (Shape function):            %d\n"
                   "d (Depth):                     %d\n"
                   "-------------------------------------------------------------\n",
                   type, rootId, b_0, shape_fn, gen_mx);
        } else if (type == BIN) {
            printf("t (Tree type):                 Binomial (%d)\n"
                   "r (Seed):                      %d\n"
                   "b (# of children at root):     %f\n"
                   "m (# of children at non-root): %d\n"
                   "q (Prob for having children):  %f\n"
                   "-------------------------------------------------------------\n",
                   type, rootId, b_0, nonLeafBF, nonLeafProb);
        } else {
            assert(0); // TODO:
        }

        printf("uth options:\n");
        uth::print_options(stdout);
        printf("=============================================================\n\n");

        fflush(stdout);
    }

    for (int i = 0; i < numRepeats; i++) {
        Result r = uts_fj_main(&walltime);

        if (me == 0) {
            maxTreeDepth = r.maxdepth;
            nNodes = r.size;
            nLeaves = r.leaves;

            double perf = (double)nNodes / walltime;

            printf("[%d] %ld ns %.6g Gnodes/s ( nodes: %llu depth: %llu leaves: %llu )\n",
                   i, walltime, perf, nNodes, maxTreeDepth, nLeaves);
        }
    }
}

int main(int argc, char **argv)
{
    uth::start(real_main, argc, argv);
    return 0;
}
