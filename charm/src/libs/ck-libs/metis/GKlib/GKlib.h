/*
 * GKlib.h
 * 
 * George's library of most frequently used routines
 *
 * $Id: GKlib.h 13005 2012-10-23 22:34:36Z karypis $
 *
 * This file in metis was modified by Kavitha Chandrasekar at UIUC
 */

#ifndef _GKLIB_H_
#define _GKLIB_H_ 1

#include "conv-config.h"

#define GKMSPACE

#if defined(_MSC_VER)
#define __MSC__
#ifndef USE_GKREGEX
#define USE_GKREGEX
#endif
#define WIN32
#endif
#if defined(__ICC)
#define __ICC__
#endif

/*
 * KEEPINSYNC: converse.h
 */
#if defined _MSC_VER
# define CMK_THREADLOCAL __declspec(thread)
#else
# define CMK_THREADLOCAL __thread
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#include "gk_arch.h" /*!< This should be here, prior to the includes */


/*************************************************************************
* Header file inclusion section
**************************************************************************/
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <setjmp.h>
#include <assert.h>
#include <sys/stat.h>

#if defined(__WITHPCRE__)
  #include <pcreposix.h>
#else
  #if defined(USE_GKREGEX)
    #include "gkregex.h"
  #else
    #include <regex.h>
  #endif /* defined(USE_GKREGEX) */
#endif /* defined(__WITHPCRE__) */



#if defined(__OPENMP__) 
#include <omp.h>
#endif




#include <gk_types.h>
#include <gk_struct.h>
#include <gk_externs.h>
#include <gk_defs.h>
#include <gk_macros.h>
#include <gk_getopt.h>

#include <gk_mksort.h>
#include <gk_mkblas.h>
#include <gk_mkmemory.h>
#include <gk_mkpqueue.h>
#include <gk_mkpqueue2.h>
#include <gk_mkrandom.h>
#include <gk_mkutils.h>

#include <gk_proto.h>


#endif  /* GKlib.h */


