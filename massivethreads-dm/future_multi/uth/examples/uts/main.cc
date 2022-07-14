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

#ifdef _OPENMP
#include <omp.h>
#define PARALLEL         1
#define COMPILER_TYPE    1
#define SHARED
#define SHARED_INDEF
#define VOLATILE         volatile
#define MAX_THREADS       32
#define LOCK_T           omp_lock_t
#define GET_NUM_THREADS  omp_get_num_threads()
#define GET_THREAD_NUM   omp_get_thread_num()
#define SET_LOCK(zlk)    omp_set_lock(zlk)
#define UNSET_LOCK(zlk)  omp_unset_lock(zlk)
#define INIT_LOCK(zlk)   zlk=omp_global_lock_alloc()
#define INIT_SINGLE_LOCK(zlk) zlk=omp_global_lock_alloc()
#define SMEMCPY          memcpy
#define ALLOC            malloc
#define BARRIER
// OpenMP helper function to match UPC lock allocation semantics
omp_lock_t * omp_global_lock_alloc() {
  omp_lock_t *lock = (omp_lock_t *) malloc(sizeof(omp_lock_t) + 128);
  omp_init_lock(lock);
  return lock;
}
#else
/***********************************************************
 *     (default) ANSI C compiler - sequential execution    *
 ***********************************************************/
#define PARALLEL         0
#define COMPILER_TYPE    0
#define SHARED
#define SHARED_INDEF
#define VOLATILE
#define MAXTHREADS 1
#define LOCK_T           void
#define GET_NUM_THREADS  1
#define GET_THREAD_NUM   0
#define SET_LOCK(zlk)    
#define UNSET_LOCK(zlk)  
#define INIT_LOCK(zlk) 
#define INIT_SINGLE_LOCK(zlk) 
#define SMEMCPY          memcpy
#define ALLOC            malloc
#define BARRIER           
#endif
/***********************************************************
 *  Global state                                           *
 ***********************************************************/
//counter_t nNodes = 0;
//counter_t nLeaves = 0;
//counter_t maxTreeDepth = 0;

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

static Result parTreeSearch_fj_seq(counter_t depth, Node parent,
                                   size_t task_depth);

static Result
doParTreeSearch_fj_seq(counter_t depth, Node parent, int childType,
                       counter_t numChildren, counter_t begin, counter_t end,
                       size_t task_depth)
{
    if (end - begin == 1) {
        Node child = makeChild(&parent, childType, computeGranularity, begin);
        return parTreeSearch_fj_seq(depth, child, task_depth);
    } else {
        counter_t center = (begin + end) / 2;

        Result r0 = doParTreeSearch_fj_seq(depth, parent, childType,
                                           numChildren, begin, center,
                                           task_depth + 1);

        Result r1 = doParTreeSearch_fj_seq(depth, parent, childType,
                                           numChildren, center, end,
                                           task_depth);
        return mergeResult(r0, r1);
    }
}

static Result parTreeSearch_fj_seq(counter_t depth, Node parent,
                               size_t task_depth) {
    Result result;

    assert(depth == 0 || parent.height > 0);

    counter_t numChildren = uts_numChildren(&parent);
    int childType = uts_childType(&parent);

    // Recurse on the children
    if (numChildren == 0) {
        Result r = { depth, 1, 1 };
        result = r;
    } else {

        result = doParTreeSearch_fj_seq(depth + 1, parent, childType,
                                        numChildren, 0, numChildren,
                                        task_depth);
        
        result.size += 1;
    }

    return result;
}

//-- parallel ---------------------------------------------------------------

static Result parTreeSearch_fj(counter_t depth, Node parent,
                               size_t task_depth) {
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
                        return parTreeSearch_fj(depth, child, task_depth);
                    },
                    mergeResult);
        
        result.size += 1;
    }

    return result;
}

//-- main ---------------------------------------------------------------------

Result uts_fj_run(counter_t depth, Node parent, size_t task_depth)
{
    uth::thread<Result> f(parTreeSearch_fj, depth, parent, task_depth);
    return f.join();
}

static Result
uts_fj_main(double *walltime)
{
    Result r;
    Node root;
    uth::pid_t me = uth::get_pid();

    if (me == 0) {
        uts_initRoot(&root, type);

        double t1 = uts_wctime();

        r = uts_fj_run((counter_t)0, root, (size_t)0);

        double t2 = uts_wctime();

        *walltime = t2 - t1;
    }
    uth::barrier();

    return r;
}

void real_main(int argc, char *argv[]) {
    counter_t nNodes = 0;
    counter_t nLeaves = 0;
    counter_t maxTreeDepth = 0;
    double walltime = 0.0;

    uts_parseParams(argc, argv);

    size_t n_procs = uth::get_n_procs();
    uth::pid_t me = uth::get_pid();

    if (me == 0) {
//        madm_conf_dump_to_file(stdout);

        if (type == GEO) {
            printf("type = %d, seed = %d, branch = %f, shape = %d,\n"
                   "depth = %d, sync = %d,\n",
                   type, rootId, b_0, shape_fn, gen_mx, sync_type);
        } else if (type == BIN) {
            printf("type = %d, seed = %d, rootbranch = %f, branch = %d,\n"
                   "prob = %f, sync = %d,\n",
                   type, rootId, b_0, nonLeafBF, nonLeafProb, sync_type);
        } else {
            assert(0); // TODO:
        }
        uth::print_options(stdout);

        fflush(stdout);
    }
    
    Result r;
#if 0
    if (sync_type == INDEP) {
        r = uts_it_main(&walltime);
    } else if (sync_type == FORKJOIN) {
        r = uts_fj_main(&walltime);
    } else {
        assert(0);
    }
#else
    r = uts_fj_main(&walltime);
#endif
    if (me == 0) {
        maxTreeDepth = r.maxdepth;
        nNodes = r.size;
        nLeaves = r.leaves;
        
        double perf = (double)nNodes / walltime * 1e-6;

        size_t server_mod = madi::comm::get_server_mod();

        printf("tree size = %llu, depth = %llu, leaves = %llu, "
               "ratio = %5.2lf,\n"
               "np = %zu, server_mod = %zu, time = %.6f, throughput = %.6f, "
               "throughput/np = %.6f,\n",
               nNodes, maxTreeDepth, nLeaves,
               (double)nLeaves/(double)nNodes*100.0,
               n_procs, server_mod, walltime, perf, perf / (double)n_procs);
        
//        uts_showStats(GET_NUM_THREADS, 0, t2 - t1, nNodes, nLeaves,
//                      maxTreeDepth);
    }

    // output logs
//    madm_prof_dump();
//    madm_log_dump();
}

int main(int argc, char **argv)
{
    uth::start(real_main, argc, argv);
    return 0;
}
