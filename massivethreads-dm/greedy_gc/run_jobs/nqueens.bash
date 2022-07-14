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

n=14

for i in $(seq 1 $nodes); do
  madmrun_opts="-n $((cores * i)) -c $cores"

  echo
  echo "## Running on $i nodes"
  echo
  numactl $numaopt ./misc/madmrun/madmrun $madmrun_opts ./uth/examples/nqueens/nqueens $n
done
