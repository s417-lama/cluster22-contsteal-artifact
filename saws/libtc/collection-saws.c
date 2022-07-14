/*********************************************************/
/*                                                       */
/*  collection-saws.c - atomic work stealing TC impl     */
/*    (c) 2021 see COPYRIGHT in top-level                */
/*                                                       */
/*********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "tc.h"
#include "saws_shrb.h"

/**
 * Create a new task collection.  Collective call.
 *
 * @param[in] max_body_size Max size of a task descriptor's body in bytes for this tc.
 *                         Any task that is added must be smaller or equal to this size.
 * @param[in] shrb_size    Size of the local task queue (in tasks).
 * @param[in] cfg          Load balancer configuation.  NULL for default configuration.
 *
 * @return                 Portable task collection handle.
 */
gtc_t gtc_create_saws(gtc_t gtc, int max_body_size, int shrb_size, gtc_ldbal_cfg_t *cfg) {
  GTC_ENTRY();
  tc_t  *tc;

  UNUSED(max_body_size);
  UNUSED(cfg);

  tc  = gtc_lookup(gtc);

  // Allocate the shared ring buffer.  Total task size is the size
  // of the header + max_body size.
  tc->shared_rb = saws_shrb_create(tc->max_body_size + sizeof(task_t), shrb_size, tc);
  tc->inbox = NULL;

  tc->cb.destroy                = gtc_destroy_saws;
  tc->cb.reset                  = gtc_reset_saws;
  tc->cb.get_buf                = gtc_get_buf_saws;
  tc->cb.add                    = gtc_add_saws;
  tc->cb.inplace_create_and_add = gtc_task_inplace_create_and_add_saws;
  tc->cb.inplace_ca_finish      = gtc_task_inplace_create_and_add_finish_saws;
  tc->cb.progress               = gtc_progress_saws;
  tc->cb.tasks_avail            = gtc_tasks_avail_saws;
  tc->cb.queue_name             = gtc_queue_name_saws;
  tc->cb.print_stats            = gtc_print_stats_saws;
  tc->cb.print_gstats           = gtc_print_gstats_saws;

  tc->rcb.pop_head               = saws_shrb_pop_head;
  tc->rcb.pop_n_tail             = saws_shrb_pop_n_tail;
  tc->rcb.try_pop_n_tail         = saws_shrb_try_pop_n_tail;
  tc->rcb.push_n_head            = saws_shrb_push_n_head;
  tc->rcb.work_avail             = saws_shrb_size;

  tc->qsize = sizeof(saws_shrb_t);

  shmem_barrier_all();

  GTC_EXIT(gtc);
}



/**
 * Destroy task collection.  Collective call.
 */
void gtc_destroy_saws(gtc_t gtc) {
  GTC_ENTRY();
  tc_t *tc = gtc_lookup(gtc);

  saws_shrb_destroy(tc->shared_rb);
  GTC_EXIT();
}



/**
 * Reset a task collection so it can be reused and removes any remaining tasks.
 * Collective call.
 */
void gtc_reset_saws(gtc_t gtc) {
  GTC_ENTRY();
  tc_t *tc = gtc_lookup(gtc);
  saws_shrb_reset(tc->shared_rb);
  GTC_EXIT();
}



/**
 * String that gives the name of this queue
 */
char *gtc_queue_name_saws() {
  GTC_ENTRY();
  GTC_EXIT("SAWS Atomic");
}



/** Invoke the progress engine.  Update work queues, balance the schedule,
 *  make progress on communication.
 */
void gtc_progress_saws(gtc_t gtc) {
  GTC_ENTRY();
  tc_t *tc = gtc_lookup(gtc);
  static int cc = 0;
  TC_START_TIMER(tc,progress);

#if 0 /* no task pushing */
  // Check the inbox for new work
  if (shrb_size(tc->inbox) > 0) {
    int   ntasks, npopped;
    void *work;

    ntasks = 100;
    work   = gtc_malloc((tc->max_body_size+sizeof(task_t))*ntasks);
    npopped= shrb_pop_n_tail(tc->inbox, _c->rank, ntasks, work, STEAL_CHUNK);

    saws_shrb_push_n_head(tc->shrb, _c->rank, work, npopped);
    shrb_free(work);

    gtc_lprintf(DBGINBOX, "gtc_progress: Moved %d tasks from inbox to my queue\n", npopped);
  }
#endif /* no task pushing */

  // Update the split
  if (saws_shrb_size(tc->shared_rb) > 1)
    saws_shrb_release(tc->shared_rb);

  // Attempt to reclaim space
  if ((cc++ % ((saws_shrb_t *)tc->shared_rb)->reclaimfreq) == 0)
    saws_shrb_reclaim_space(tc->shared_rb);
  ((saws_shrb_t *)tc->shared_rb)->nprogress++;
  TC_STOP_TIMER(tc,progress);
  GTC_EXIT();
}



/**
 * Number of tasks available in local task collection.  Note, this is an
 * approximate number since we're not locking the data structures.
 */
int gtc_tasks_avail_saws(gtc_t gtc) {
  GTC_ENTRY();
  tc_t *tc = gtc_lookup(gtc);

  //return saws_shrb_size(tc->shrb) + shrb_size(tc->inbox);
  GTC_EXIT(saws_shrb_size(tc->shared_rb));
}


/**
 * Find work to do, search everywhere. Use when you write your own
 * gtc_process() implementation.  Do NOT use together with
 * gtc_process().  This function invokes load balancing to attempt to locate
 * work when none is available locally.  It returns NULL only when global
 * termination has been detected.
 *
 * @param tc       IN Ptr to task collection
 * @return         Ptr to task (from local queue or stolen). NULL if none
 *                 found.  A NULL result here means that global termination
 *                 has occurred.  Returned buffer should be deleted by the user.
 */
double gtc_get_dw = 0.0;

//static inline int max_steals(uint64_t itasks) {
//  uint32_t curr,cnt, total = 0;

//  for (cnt = 0, curr = itasks; total != itasks; cnt++) {
//    curr = (curr != 1) ? curr >> 1 : 1;
//    total += curr;
//    curr = itasks - total;
//  }
//  return cnt;
//}


int gtc_get_buf_saws(gtc_t gtc, int priority, task_t *buf) {
  GTC_ENTRY();
  tc_t   *tc = gtc_lookup(gtc);
  int     got_task = 0;
  int     v, steal_size;
  int     passive = 0;
  int     searching = 0;
  gtc_vs_state_t vs_state = {0, 0, 0};

  tc->ct.getcalls++;
  TC_START_TIMER(tc, getbuf);

  // Invoke the progress engine
  gtc_progress(gtc);

  // Try to take my own work first.  We take from the head of our own queue.
  // When we steal, we take work off of the tail of the target's queue.
  got_task = gtc_get_local_buf(gtc, priority, buf);

  // Time dispersion.  If I had work to start this should be ~0.
  if (!tc->dispersed) TC_START_TIMER(tc, dispersion);

  // No more local work, try to steal some
  if (!got_task && tc->ldbal_cfg.stealing_enabled) {
    gtc_lprintf(DBGGET, " Thread %d: gtc_get() searching for work\n", _c->rank);

#ifndef NO_SEATBELTS
    TC_START_TIMER(tc, passive);
    TC_INIT_TIMER(tc, imbalance);
    TC_START_TIMER(tc, imbalance);
    passive = 1;
    tc->ct.passive_count++;
#endif

    // Keep searching until we find work or detect termination
    while (!got_task && !tc->terminated) {

      tc->state = STATE_SEARCHING;

      if (!searching) {
        TC_START_TIMER(tc, search);
        searching = 1;
      }

      // Select the next target
      v = gtc_select_target(gtc, &vs_state);

      tc->state = STATE_STEALING;

      // attempt remote steal
      steal_size = gtc_steal_tail(gtc, v);
      // Steal succeeded: Got some work from remote node
      if (steal_size > 0) {
        tc->ct.tasks_stolen += steal_size;
        tc->ct.num_steals += 1;
        tc->last_target = v;
        searching = 1;

        // Steal failed: Got the lock, no longer any work on remote node
      } else {
        tc->ct.failed_steals_unlocked++;
      }

      // Invoke the progress engine
      gtc_progress(gtc);

      // Still no work? Lock to be sure and check for termination.
      // Locking is only needed here if we allow pushing.
      // TODO: New TD should not require locking.  Remove locks and test.
      if (gtc_tasks_avail(gtc) == 0 && !tc->external_work_avail) {
        td_set_counters(tc->td, tc->ct.tasks_spawned, tc->ct.tasks_completed);
        tc->terminated = td_attempt_vote(tc->td);

      // We have work, done stealing
      } else if (gtc_tasks_avail(gtc)) {
        got_task = gtc_get_local_buf(gtc, priority, buf);
      }
    } //end whileloop for td

  } else {
    tc->ct.getlocal++;
  }

#ifndef NO_SEATBELTS
  if (passive) TC_STOP_TIMER(tc, passive);
  if (passive) TC_STOP_TIMER(tc, imbalance);
  //if (searching) TC_STOP_TIMER(tc, search);
#endif

  // Record how many attempts it took for our first get, this is the number of
  // attempts during the work dispersion phase.
  if (!tc->dispersed) {
    //if (passive) TC_STOP_TIMER(tc, dispersion);
    tc->dispersed = 1;
    tc->ct.dispersion_attempts_unlocked = tc->ct.failed_steals_unlocked;
    tc->ct.dispersion_attempts_locked   = tc->ct.failed_steals_locked;
  }

  gtc_lprintf(DBGGET, " Thread %d: gtc_get() %s\n", _c->rank, got_task? "got work":"no work");
  if (got_task) tc->state = STATE_WORKING;
  TC_STOP_TIMER(tc,getbuf);
  GTC_EXIT(got_task);
}



/**
 * Add task to the task collection.  Task is copied in and task buffer is available
 * to the user when call returns.  Non-collective call.
 *
 * @param tc      IN Ptr to task collection
 * @param proc     IN Process # whose task collection this task is to be added
 *                    to. Common case is tc->procid
 * @param task  INOUT Task to be added. user manages buffer when call
 *                    returns. Preferably allocated in ARMCI local allocated memory when
 *                    proc != tc->procid for improved RDMA performance.  This call fills
 *                    in task field and the contents of task will match what is in the queue
 *                    when the call returns.
 *
 * @return 0 on success.
 */
int gtc_add_saws(gtc_t gtc, task_t *task, int proc) {
  GTC_ENTRY();
  tc_t *tc = gtc_lookup(gtc);

  assert(gtc_task_body_size(task) <= tc->max_body_size);
  assert(tc->state != STATE_TERMINATED);
  TC_START_TIMER(tc,add);

  task->created_by = _c->rank;

  if (proc == _c->rank) {
    // Local add: put it straight onto the local work list
    saws_shrb_push_head(tc->shared_rb, _c->rank, task, sizeof(task_t) + gtc_task_body_size(task));
  }
#if 0 /* no task pushing */
  else {
    // Remote adds: put this in the remote node's inbox
    if (task->affinity == 0)
      shrb_push_head(tc->inbox, proc, task, sizeof(task_t) + gtc_task_body_size(task));
    else
      shrb_push_tail(tc->inbox, proc, task, sizeof(task_t) + gtc_task_body_size(task));
  }
#endif /* no task pushing */

  ++tc->ct.tasks_spawned;
  TC_STOP_TIMER(tc,add);
  GTC_EXIT(0);
}


/**
 * Create-and-add a task in-place on the head of the queue.  Note, you should
 * not do *ANY* other queue operations until all outstanding in-place creations
 * have finished.  The pointer returned points directly to an element in the
 * queue.  Do not add it, do not free it, discard the pointer when you are finished
 * assigning the task body.
 *
 * @param gtc    Portable reference to the task collection
 * @param tclass Desired task class
 */
task_t *gtc_task_inplace_create_and_add_saws(gtc_t gtc, task_class_t tclass) {
  GTC_ENTRY();
  tc_t   *tc = gtc_lookup(gtc);
  task_t *t;
  TC_START_TIMER(tc,addinplace);

  //assert(gtc_group_steal_ismember(gtc)); // Only masters can do this

  t = (task_t*) saws_shrb_alloc_head(tc->shared_rb);
  gtc_task_set_class(t, tclass);

  t->created_by = _c->rank;
  //t->affinity   = 0;
  t->priority   = 0;

  ++tc->ct.tasks_spawned;

  TC_STOP_TIMER(tc,addinplace);

  GTC_EXIT(t);
}


/**
 * Complete an in-place task creation.  Note, you should not do *ANY* other
 * queue operations until all outstanding in-place creations have finished.
 *
 * @param gtc    Portable reference to the task collection
 * @param task   The pointer that was returned by inplace_create_and_add()
 */
void gtc_task_inplace_create_and_add_finish_saws(gtc_t gtc, task_t *t) {
  GTC_ENTRY();
  tc_t *tc = gtc_lookup(gtc);
  // TODO: Maintain a counter of how many are outstanding to avoid corruption at the
  // head of the queue
  TC_START_TIMER(tc,addfinish);

  // Can't release until the inplace op completes
  gtc_progress_saws(gtc);
  TC_STOP_TIMER(tc,addfinish);
  GTC_EXIT();
}


/**
 * Print stats for this task collection.
 * @param tc       IN Ptr to task collection
 */
void gtc_print_stats_saws(gtc_t gtc) {
  GTC_ENTRY();
  tc_t *tc = gtc_lookup(gtc);
  saws_shrb_t *rb = tc->shared_rb;

  uint64_t perget, peradd, perinplace, perfinish, perprogress, perreclaim, perensure, perrelease, perreacquire, perpoptail;

  if (!getenv("SCIOTO_DISABLE_STATS") && !getenv("SCIOTO_DISABLE_PERNODE_STATS")) {
    // avoid floating point exceptions...
    perget       = tc->ct.getcalls      != 0 ? TC_READ_TIMER(tc,getbuf)    / tc->ct.getcalls      : 0;
    peradd       = tc->ct.tasks_spawned != 0 ? TC_READ_TIMER(tc,add)       / tc->ct.tasks_spawned : 0;
    perinplace   = tc->ct.tasks_spawned != 0 ? TC_READ_TIMER(tc,addinplace)/ tc->ct.tasks_spawned : 0; // borrowed
    perfinish    = rb->nprogress     != 0 ? TC_READ_TIMER(tc,addfinish) / rb->nprogress     : 0; // borrowed, but why?
    perprogress  = rb->nprogress     != 0 ? TC_READ_TIMER(tc,progress)  / rb->nprogress     : 0;
    perreclaim   = rb->nreccalls     != 0 ? TC_READ_TIMER(tc,reclaim)   / rb->nreccalls     : 0;
    perensure    = rb->nensure       != 0 ? TC_READ_TIMER(tc,ensure)    / rb->nensure       : 0;
    perrelease   = rb->nrelease      != 0 ? TC_READ_TIMER(tc,release)   / rb->nrelease      : 0;
    perreacquire = rb->nreacquire    != 0 ? TC_READ_TIMER(tc,reacquire) / rb->nreacquire    : 0;
    perpoptail   = rb->ngets         != 0 ? TC_READ_TIMER(tc,poptail)   / rb->ngets         : 0;

    printf(" %4d - saws-Q: nrelease %6lu, nreacquire %6lu, nreclaimed %6lu, nwaited %2lu, nprogress %6lu\n"
           " %4d -    failed w/lock: %6lu, failed w/o lock: %6lu, aborted steals: %6lu\n"
           " %4d -    ngets: %6lu  (%5.2f usec/get) nxfer: %6lu\n",
      _c->rank,
        rb->nrelease, rb->nreacquire, rb->nreclaimed, rb->nwaited, rb->nprogress,
      _c->rank,
        tc->ct.failed_steals_locked, tc->ct.failed_steals_unlocked, tc->ct.aborted_steals,
      _c->rank,
        rb->ngets, TC_READ_TIMER_USEC(tc, t[0])/(double)rb->ngets, rb->nxfer);
    printf(" %4d - TSC: get: %"PRIu64"M (%"PRIu64" x %"PRIu64")  add: %"PRIu64"M (%"PRIu64" x %"PRIu64") inplace: %"PRIu64"M (%"PRIu64")\n",
        _c->rank,
        TC_READ_TIMER_M(tc,getbuf), perget, tc->ct.getcalls,
        TC_READ_TIMER_M(tc,add), peradd, tc->ct.tasks_spawned,
        TC_READ_TIMER_M(tc,addinplace), perinplace);
    printf(" %4d - TSC: addfinish: %"PRIu64"M (%"PRIu64") progress: %"PRIu64"M (%"PRIu64" x %"PRIu64") reclaim: %"PRIu64"M (%"PRIu64" x %"PRIu64")\n",
        _c->rank,
        TC_READ_TIMER_M(tc,addfinish), perfinish,
        TC_READ_TIMER_M(tc,progress), perprogress, rb->nprogress,
        TC_READ_TIMER_M(tc,reclaim), perreclaim, rb->nreccalls);
    printf(" %4d - TSC: ensure: %"PRIu64"M (%"PRIu64" x %"PRIu64") release: %"PRIu64"M (%"PRIu64" x %"PRIu64") "
           "reacquire: %"PRIu64"M (%"PRIu64" x %"PRIu64")\n",
        _c->rank,
        TC_READ_TIMER_M(tc,ensure), perensure, rb->nensure,
        TC_READ_TIMER_M(tc,release), perrelease, rb->nrelease,
        TC_READ_TIMER_M(tc,reacquire), perreacquire, rb->nreacquire);
    printf(" %4d - TSC: pushhead: %"PRIu64"M (%"PRIu64") poptail: %"PRIu64"M (%"PRIu64" x %"PRIu64")\n",
        _c->rank,
        TC_READ_TIMER_M(tc,pushhead), (uint64_t)0,
        TC_READ_TIMER_M(tc,poptail), perpoptail, rb->ngets);
  }
  GTC_EXIT();
}



/**
 * Print global stats for this task collection.
 * @param tc       IN Ptr to task collection
 */
void gtc_print_gstats_saws(gtc_t gtc) {
  GTC_ENTRY();
  tc_t *tc = gtc_lookup(gtc);
  saws_shrb_t *rb = (saws_shrb_t *)tc->shared_rb;
  double   *times, *mintimes, *maxtimes, *sumtimes;
  long     *counts, *mincounts, *maxcounts, *sumcounts;

  int ntimes = 16;
  times     = gtc_shmem_calloc(ntimes, sizeof(double));
  mintimes  = gtc_shmem_calloc(ntimes, sizeof(double));
  maxtimes  = gtc_shmem_calloc(ntimes, sizeof(double));
  sumtimes  = gtc_shmem_calloc(ntimes, sizeof(double));

  int ncounts = 13;
  counts     = gtc_shmem_calloc(ncounts, sizeof(long));
  mincounts  = gtc_shmem_calloc(ncounts, sizeof(long));
  maxcounts  = gtc_shmem_calloc(ncounts, sizeof(long));
  sumcounts  = gtc_shmem_calloc(ncounts, sizeof(long));


  times[SAWSPopTailTime]        = TC_READ_TIMER_MSEC(tc,poptail);
  times[SAWSGetMetaTime]        = TC_READ_TIMER_MSEC(tc,getmeta);
  times[SAWSProgressTime]       = TC_READ_TIMER_USEC(tc,progress);
  times[SAWSReclaimTime]        = TC_READ_TIMER_USEC(tc,reclaim);
  times[SAWSEnsureTime]         = TC_READ_TIMER_USEC(tc,ensure);
  times[SAWSReacquireTime]      = TC_READ_TIMER_MSEC(tc,reacquire);
  times[SAWSReleaseTime]        = TC_READ_TIMER_USEC(tc,release);
  times[SAWSPerPopTailTime]     = rb->ngets         != 0 ? TC_READ_TIMER_MSEC(tc,poptail)   / rb->ngets         : 0.0;
  times[SAWSPerGetMetaTime]     = rb->nmeta         != 0 ? TC_READ_TIMER_MSEC(tc,getmeta)   / rb->nmeta         : 0.0;
  times[SAWSPerProgressTime]    = rb->nprogress     != 0 ? TC_READ_TIMER_USEC(tc,progress)  / rb->nprogress     : 0.0;
  times[SAWSPerReclaimTime]     = rb->nreccalls     != 0 ? TC_READ_TIMER_USEC(tc,reclaim)   / rb->nreccalls     : 0.0;
  times[SAWSPerEnsureTime]      = rb->nensure       != 0 ? TC_READ_TIMER_USEC(tc,ensure)    / rb->nensure       : 0.0;
  times[SAWSPerReacquireTime]   = rb->nreacquire    != 0 ? TC_READ_TIMER_MSEC(tc,reacquire) / rb->nreacquire    : 0.0;
  times[SAWSPerReleaseTime]     = rb->nrelease      != 0 ? TC_READ_TIMER_USEC(tc,release)   / rb->nrelease      : 0.0;
  times[14]			= TC_READ_TIMER_USEC(tc, t[0]);
  times[15]			= TC_READ_TIMER_USEC(tc, t[1]);
  counts[SAWSNumGets]            = rb->ngets;
  counts[SAWSGetCalls]           = tc->ct.getcalls;
  counts[SAWSNumMeta]            = rb->nmeta;
  counts[SAWSGetLocalCalls]      = tc->ct.getlocal;
  counts[SAWSNumSteals]          = rb->nsteals;
  counts[SAWSStealFailsLocked]   = tc->ct.failed_steals_locked;
  counts[SAWSStealFailsUnlocked] = tc->ct.failed_steals_unlocked;
  counts[SAWSAbortedSteals]      = tc->ct.aborted_steals;
  counts[SAWSProgressCalls]      = rb->nprogress;
  counts[SAWSReclaimCalls]       = rb->nreccalls;
  counts[SAWSEnsureCalls]        = rb->nensure;
  counts[SAWSReacquireCalls]     = rb->nreacquire;
  counts[SAWSReleaseCalls]       = rb->nrelease;

#if (SHMEM_MAJOR_VERSION == 1) && (SHMEM_MINOR_VERSION >= 5)
  shmem_min_reduce(SHMEM_TEAM_WORLD, mintimes, times, ntimes);
  shmem_max_reduce(SHMEM_TEAM_WORLD, maxtimes, times, ntimes);
  shmem_sum_reduce(SHMEM_TEAM_WORLD, sumtimes, times, ntimes);

  shmem_min_reduce(SHMEM_TEAM_WORLD, mincounts, counts, ncounts);
  shmem_max_reduce(SHMEM_TEAM_WORLD, maxcounts, counts, ncounts);
  shmem_sum_reduce(SHMEM_TEAM_WORLD, sumcounts, counts, ncounts);
#else
  shmem_double_min_to_all(mintimes, times, ntimes, 0, 0, _c->size, (double*)_c->pWrk, _c->pSync);
  shmem_double_max_to_all(maxtimes, times, ntimes, 0, 0, _c->size, (double*)_c->pWrk, _c->pSync);
  shmem_double_sum_to_all(sumtimes, times, ntimes, 0, 0, _c->size, (double*)_c->pWrk, _c->pSync);

  shmem_long_min_to_all(mincounts, counts, ncounts, 0, 0, _c->size, (long*)_c->pWrk, _c->pSync);
  shmem_long_max_to_all(maxcounts, counts, ncounts, 0, 0, _c->size, (long*)_c->pWrk, _c->pSync);
  shmem_long_sum_to_all(sumcounts, counts, ncounts, 0, 0, _c->size, (long*)_c->pWrk, _c->pSync);
#endif
  shmem_barrier_all();

  eprintf("        : shared heap memory allocated: %d    local heap memory allocated: %d\n", _c->shmallocsize, _c->allocsize);

  eprintf("        : gets         %6lu (%6.2f/%3lu/%3lu) time %6.2fms/%6.2fms/%6.2fms per %6.2fms/%6.2fms/%6.2fms\n",
      sumcounts[SAWSNumGets], sumcounts[SAWSNumGets]/(double)_c->size, mincounts[SAWSNumGets], maxcounts[SAWSNumGets],
      sumtimes[SAWSPopTailTime]/_c->size, mintimes[SAWSPopTailTime], maxtimes[SAWSPopTailTime],
      sumtimes[SAWSPerPopTailTime]/_c->size, mintimes[SAWSPerPopTailTime], maxtimes[SAWSPerPopTailTime]);

  eprintf("        :   get_buf    %6lu (%6.2f/%3lu/%3lu\n",
      sumcounts[SAWSGetCalls], sumcounts[SAWSGetCalls]/(double)_c->size, mincounts[SAWSGetCalls], maxcounts[SAWSGetCalls]);

  eprintf("        :   get_meta   %6lu (%6.2f/%3lu/%3lu) time %6.2fms/%6.2fms/%6.2fms per %6.2fms/%6.2fms/%6.2fms\n",
      sumcounts[SAWSNumMeta], sumcounts[SAWSNumMeta]/(double)_c->size, mincounts[SAWSNumMeta], maxcounts[SAWSNumMeta],
      sumtimes[SAWSGetMetaTime]/_c->size, mintimes[SAWSGetMetaTime], maxtimes[SAWSGetMetaTime],
      sumtimes[SAWSPerGetMetaTime]/_c->size, mintimes[SAWSPerGetMetaTime], maxtimes[SAWSPerGetMetaTime]);

  eprintf("        :   localget   %6lu (%6.2f/%3lu/%3lu)\n",
      sumcounts[SAWSGetLocalCalls], sumcounts[SAWSGetLocalCalls]/(double)_c->size,
      mincounts[SAWSGetLocalCalls], maxcounts[SAWSGetLocalCalls]);
  eprintf("        :   steals     %6lu (%6.2f/%3lu/%3lu)\n",
      sumcounts[SAWSNumSteals], sumcounts[SAWSNumSteals]/(double)_c->size,
      mincounts[SAWSNumSteals], maxcounts[SAWSNumSteals]);
  eprintf("        :   fails lock %6lu (%6.2f/%3lu/%3lu)\n",
      sumcounts[SAWSStealFailsLocked], sumcounts[SAWSStealFailsLocked]/(double)_c->size,
      mincounts[SAWSStealFailsLocked], maxcounts[SAWSStealFailsLocked]);
  eprintf("        :   fails un   %6lu (%6.2f/%3lu/%3lu)\n",
      sumcounts[SAWSStealFailsUnlocked], sumcounts[SAWSStealFailsUnlocked]/(double)_c->size,
      mincounts[SAWSStealFailsUnlocked], maxcounts[SAWSStealFailsUnlocked]);
  eprintf("        :   fails ab   %6lu (%6.2f/%3lu/%3lu)\n",
      sumcounts[SAWSAbortedSteals], sumcounts[SAWSAbortedSteals]/(double)_c->size,
      mincounts[SAWSAbortedSteals], maxcounts[SAWSAbortedSteals]);

  eprintf("        : progress   %6.2f/%3lu/%3lu time %6.2fus/%6.2fus/%6.2fus per %6.2fus/%6.2fus/%6.2fus\n",
      sumcounts[SAWSProgressCalls]/(double)_c->size, mincounts[SAWSProgressCalls], maxcounts[SAWSProgressCalls],
      sumtimes[SAWSProgressTime]/_c->size, mintimes[SAWSProgressTime], maxtimes[SAWSProgressTime],
      sumtimes[SAWSPerProgressTime]/_c->size, mintimes[SAWSPerProgressTime], maxtimes[SAWSPerProgressTime]);
  eprintf("        : reclaim    %6.2f/%3lu/%3lu time %6.2fus/%6.2fus/%6.2fus per %6.2fus/%6.2fus/%6.2fus\n",
      sumcounts[SAWSReclaimCalls]/(double)_c->size, mincounts[SAWSReclaimCalls], maxcounts[SAWSReclaimCalls],
      sumtimes[SAWSReclaimTime]/_c->size, mintimes[SAWSReclaimTime], maxtimes[SAWSReclaimTime],
      sumtimes[SAWSPerReclaimTime]/_c->size, mintimes[SAWSPerReclaimTime], maxtimes[SAWSPerReclaimTime]);
  eprintf("        : ensure     %6.2f/%3lu/%3lu time %6.2fus/%6.2fus/%6.2fus per %6.2fus/%6.2fus/%6.2fus\n",
      sumcounts[SAWSEnsureCalls]/(double)_c->size, mincounts[SAWSEnsureCalls], maxcounts[SAWSEnsureCalls],
      sumtimes[SAWSEnsureTime]/_c->size, mintimes[SAWSEnsureTime], maxtimes[SAWSEnsureTime],
      sumtimes[SAWSPerEnsureTime]/_c->size, mintimes[SAWSPerEnsureTime], maxtimes[SAWSPerEnsureTime]);
  eprintf("        : reacquire  %6.2f/%3lu/%3lu time %6.2fms/%6.2fms/%6.2fms per %6.2fms/%6.2fms/%6.2fms\n",
      sumcounts[SAWSReacquireCalls]/(double)_c->size, mincounts[SAWSReacquireCalls], maxcounts[SAWSReacquireCalls],
      sumtimes[SAWSReacquireTime]/_c->size, mintimes[SAWSReacquireTime], maxtimes[SAWSReacquireTime],
      sumtimes[SAWSPerReacquireTime]/_c->size, mintimes[SAWSPerReacquireTime], maxtimes[SAWSPerReacquireTime]);
  eprintf("        : release    %6.2f/%3lu/%3lu time %6.2fus/%6.2fus/%6.2fus per %6.2fus/%6.2fus/%6.2fus\n",
      sumcounts[SAWSReleaseCalls]/(double)_c->size, mincounts[SAWSReleaseCalls], maxcounts[SAWSReleaseCalls],
      sumtimes[SAWSReleaseTime]/_c->size, mintimes[SAWSReleaseTime], maxtimes[SAWSReleaseTime],
      sumtimes[SAWSPerReleaseTime]/_c->size, mintimes[SAWSPerReleaseTime], maxtimes[SAWSPerReleaseTime]);

  eprintf("&&&  %6.2f %6.2f ", sumtimes[SAWSPopTailTime]/_c->size, sumtimes[SAWSReacquireTime]/_c->size);

  shmem_free(times);
  shmem_free(mintimes);
  shmem_free(maxtimes);
  shmem_free(sumtimes);

  shmem_free(counts);
  shmem_free(mincounts);
  shmem_free(maxcounts);
  shmem_free(sumcounts);
  GTC_EXIT();
}



/**
 * Delete all tasks in my patch of the task collection.  Useful when
 * simulating failure.
 */
void gtc_queue_reset_saws(gtc_t gtc) {
  GTC_ENTRY();
  tc_t *tc = gtc_lookup(gtc);

  // Clear out the ring buffer
  saws_shrb_lock(tc->shared_rb, _c->rank);
  saws_shrb_reset(tc->shared_rb);
  saws_shrb_unlock(tc->shared_rb, _c->rank);

#if 0 /* no task pushing */
  // Clear out the inbox
  shrb_lock(tc->inbox, _c->rank);
  shrb_reset(tc->inbox);
  shrb_unlock(tc->inbox, _c->rank);
#endif /* no task pushing */
  GTC_EXIT();
}
