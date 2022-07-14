#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <shmem.h>
#include <shmemx.h>

#include <tc.h>
#include "uts.h"
//}

#include "UTSIterator.h"
#include "RecursiveLoadBalancers.h"

gtc_qtype_t qtype = GtcQueueSDC;

/***********************************************************
 *  UTS Implementation Hooks                               *
 ***********************************************************/

// The name of this implementation
char * impl_getName() {
#if   defined(LDBAL_SEQUENTIAL)
  return (char *)"Sequential Recursive Search";
#elif defined(LDBAL_SCIOTO)
#ifdef INPLACE
  return (char *)"Sciotwo Parallel Search [Inplace Create-and-Add]";
#else
  return (char *)"Sciotwo Parallel Search";
#endif /* INPLACE */
#endif
}

int  impl_paramsToStr(char *strBuf, int ind) { 
  ind += sprintf(strBuf+ind, "Execution strategy:  %s\n", impl_getName());
  return ind;
}

// Not adding any UTS command line params, return non-success
int  impl_parseParam(char *param, char *value) {
  int ret = 1;
  if (param[1] == 'Q') {
    switch (value[0]) {
      case 'B':
        qtype = GtcQueueSDC;
        ret = 0;
        break;
      case 'H':
        qtype = GtcQueueSAWS;
        ret = 0;
        break;
      default:
        printf("-Q: unknown queue type must be one of 'B' 'N' or 'H'\n");
        break;
    }
  }
  return ret;
}

void impl_helpMessage() {
  printf("   none.\n");
}

void impl_abort(int err) {
  exit(err);
}

/***********************************************************
 *  UTS Parallel Iterator Implementation                   *
 ***********************************************************/


int main(int argc, char *argv[]) {
  int me, nproc;
  uint64_t t1, t2;

  gtc_init();
  me    = _c->rank;
  nproc = _c->size;
  t1 = nproc; // make c++ shut up when the conditional compilation doesn't refer to nproc

  if (me == 0) {
    printf("# of processes: %d\n", nproc);
  }
  
  uts_parseParams(argc, argv);

  if (me == 0) {
    uts_printParams();
  }
  

  UTSIterator rootIter (type);

  shmem_barrier_all();

#if   defined(LDBAL_SEQUENTIAL)
  t1 = uts_wctime();
  ldbal_sequential(rootIter);
  t2 = uts_wctime();
#elif defined(LDBAL_SCIOTO)
  t1 = 0;
  t2 = ldbal_scioto(rootIter);
#else
#error Please select a load balancer
#endif

  static long my_nNodes   = UTSIterator::get_nNodes();
  static long my_nLeaves  = UTSIterator::get_nLeaves();
  static long my_maxDepth = UTSIterator::get_maxDepth();
  static long nNodes, nLeaves, maxDepth;
#if   defined(LDBAL_SEQUENTIAL)
  nproc    = 1;
  nNodes   = my_nNodes;
  nLeaves  = my_nLeaves;
  maxDepth = my_maxDepth;
#elif defined(LDBAL_SCIOTO)
#if (SHMEM_MAJOR_VERSION == 1) && (SHMEM_MINOR_VERSION >= 5)
  shmem_sum_reduce(SHMEM_TEAM_WORLD, &nNodes, &my_nNodes, 1);
  shmem_sum_reduce(SHMEM_TEAM_WORLD, &nLeaves, &my_nLeaves, 1);
  shmem_max_reduce(SHMEM_TEAM_WORLD, &maxDepth, &my_maxDepth, 1);
#else
  shmem_long_sum_to_all(&nNodes, &my_nNodes, 1, 0, 0, _c->size, (long*)_c->pWrk, _c->pSync);
  shmem_long_sum_to_all(&nLeaves, &my_nLeaves, 1, 0, 0, _c->size, (long*)_c->pWrk, _c->pSync);
  shmem_long_max_to_all(&maxDepth, &my_maxDepth, 1, 0, 0, _c->size, (long*)_c->pWrk, _c->pSync);
#endif
#endif

  if (me == 0) uts_showStats(nproc, 0, t2-t1, nNodes, nLeaves, maxDepth);

  gtc_fini();
  return 0;
}

