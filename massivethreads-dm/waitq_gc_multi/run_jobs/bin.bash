#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

# COMM_LAYER=shmem
COMM_LAYER=mpi3

CFLAGS=""
# CFLAGS="$CFLAGS -O0 -g"

CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure --with-comm-layer=$COMM_LAYER
make -j

numaopt=-iall
# numaopt=-l

case "$MACHINE_NAME" in
  obcx) cores=56 nodes=$PJM_MPI_PROC ;;
esac

madmrun_opts="-n $((cores * nodes)) -c $cores"

depth=8
leaf_loops=10000
interm_loops=1000
interm_iters=3
pre_exec=1

numactl $numaopt ./misc/madmrun/madmrun $madmrun_opts ./uth/examples/bin/bin $depth $leaf_loops $interm_loops $interm_iters $pre_exec
