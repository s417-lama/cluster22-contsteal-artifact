#ifndef _CONV_MACH_H
#define _CONV_MACH_H

#define CMK_NO_OUTSTANDING_SENDS			   0

#define CMK_64BIT                                          1

//#define CMK_MEMORY_PREALLOCATE_HACK                        1

//#define CMK_CONVERSE_MPI                                   1

#define CMK_NO_SOCKETS					   1

#define CMK_DEFAULT_MAIN_USES_COMMON_CODE                  1

#define CMK_GETPAGESIZE_AVAILABLE                          1

#define CMK_IS_HETERO                                      0

#define CMK_MALLOC_USE_GNU_MALLOC                          0
#define CMK_MALLOC_USE_OS_BUILTIN                          1

#define CMK_MEMORY_PAGESIZE                                4096
#define CMK_MEMORY_PROTECTABLE                             1


#define CMK_SHARED_VARS_UNAVAILABLE                        1

#define CMK_SIGNAL_NOT_NEEDED                              0
#define CMK_SIGNAL_USE_SIGACTION                           0
#define CMK_SIGNAL_USE_SIGACTION_WITH_RESTART              1

#define CMK_SYNCHRONIZE_ON_TCP_CLOSE                       0

#define CMK_THREADS_USE_CONTEXT                            0
#define CMK_THREADS_USE_JCONTEXT                           1
#define CMK_THREADS_USE_PTHREADS                           0
#define CMK_THREADS_ARE_WIN32_FIBERS                       0

#define CMK_THREADS_REQUIRE_NO_CPV                         0

#define CMK_TIMER_USE_GETRUSAGE                            0
#define CMK_TIMER_USE_SPECIAL                              0
#define CMK_TIMER_USE_TIMES                                0
// This needs to be compiled with gcc only
#define CMK_TIMER_USE_BLUEGENEQ			           1

#define CMK_TYPEDEF_INT1 signed char
#define CMK_TYPEDEF_INT2 short
#define CMK_TYPEDEF_INT4 int
#define CMK_TYPEDEF_INT8 long long
#define CMK_TYPEDEF_UINT1 unsigned char
#define CMK_TYPEDEF_UINT2 unsigned short
#define CMK_TYPEDEF_UINT4 unsigned int
#define CMK_TYPEDEF_UINT8 unsigned long long
#define CMK_TYPEDEF_FLOAT4 float
#define CMK_TYPEDEF_FLOAT8 double

#define CMK_WHEN_PROCESSOR_IDLE_BUSYWAIT                   1
#define CMK_WHEN_PROCESSOR_IDLE_USLEEP                     0


#define CMK_WEB_MODE                                       1
#define CMK_DEBUG_MODE                                     0

#define CMK_LBDB_ON					   1

#define CMK_BLUEGENEQ                                      1
#define CMK_BLUEGENEQ_OPTCOPY                              1

#define CMK_NO_ISO_MALLOC                                  1

#undef CMI_DIRECT_MANY_TO_MANY_DEFINED
#define CMI_DIRECT_MANY_TO_MANY_DEFINED                    1

#endif

