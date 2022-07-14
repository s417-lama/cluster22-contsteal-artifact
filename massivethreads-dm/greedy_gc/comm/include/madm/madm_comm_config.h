#ifndef MADI_COMM_CONFIG_H
#define MADI_COMM_CONFIG_H

#ifdef __cplusplus
#include <climits>
#else
#include <limits.h>
#endif

#define MADI_COMM_LAYER_SEQ    0
#define MADI_COMM_LAYER_SHMEM  1
#define MADI_COMM_LAYER_MPI3   2
#define MADI_COMM_LAYER_GASNET 3
#define MADI_COMM_LAYER_IBV    4
#define MADI_COMM_LAYER_FX10   5


#include "madm_comm_acconfig.h"


#if MADI_COMM_LAYER == MADI_COMM_LAYER_SHMEM

#define MADI_DEFAULT_N_CORES            (INT_MAX)
#define MADI_DEFAULT_SERVER_MOD         (0)

#elif MADI_COMM_LAYER == MADI_COMM_LAYER_MPI3

#define MADI_DEFAULT_N_CORES            (16)
#define MADI_DEFAULT_SERVER_MOD         (0)

#elif MADI_COMM_LAYER == MADI_COMM_LAYER_GASNET

#define MADI_DEFAULT_N_CORES            (16)
#define MADI_DEFAULT_SERVER_MOD         (0)

#elif MADI_COMM_LAYER == MADI_COMM_LAYER_IBV

#define MADI_DEFAULT_N_CORES            (16)
#define MADI_DEFAULT_SERVER_MOD         (0)

#elif MADI_COMM_LAYER == MADI_COMM_LAYER_FX10

#define MADI_DEFAULT_N_CORES            (16)
#define MADI_DEFAULT_SERVER_MOD         (16)

#else

#error "unknown communication layer"

#endif

#endif
