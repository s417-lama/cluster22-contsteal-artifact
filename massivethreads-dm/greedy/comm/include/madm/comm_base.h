#ifndef MADI_COMM_BASE_H
#define MADI_COMM_BASE_H

#include "madm_comm_config.h"

#if MADI_COMM_LAYER == MADI_COMM_LAYER_SEQ
#include "seq/comm_base.h"
#elif MADI_COMM_LAYER == MADI_COMM_LAYER_SHMEM
#include "shmem/comm_base.h"
#elif MADI_COMM_LAYER == MADI_COMM_LAYER_MPI3
#include "mpi3/comm_base.h"
#elif MADI_COMM_LAYER == MADI_COMM_LAYER_GASNET
#include "gasnet/comm_base.h"
#elif MADI_COMM_LAYER == MADI_COMM_LAYER_IBV
#include "ibv/comm_base.h"
#elif MADI_COMM_LAYER == MADI_COMM_LAYER_FX10
#include "fjmpi/comm_base.h"
#else
#error ""
#endif

#endif
