#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..
source run_jobs/common.bash

source uts/sample_trees.sh

case $MACHINE_NAME in
  ito)
    tree=$T1L
    # tree=$T1XL
    # tree=$T1XXL
    # tree=$T1WL
    ;;
  wisteria-o)
    tree=$T1L
    # tree=$T1XL
    # tree=$T1XXL
    # tree=$T1WL
    ;;
  fugaku)
    tree=$T1L
    # tree=$T1XL
    # tree=$T1XXL
    # tree=$T1WL
    ;;
  *)
    tree=$T1L
    ;;
esac

run_seq=0

# n_repeats=1
# n_repeats=3
n_repeats=10

CXXFLAGS=""
# CXXFLAGS="$CXXFLAGS -Ofast -g"

if [[ $run_seq == 1 ]]; then
  CXXFLAGS="$CXXFLAGS -DUTS_RUN_SEQ=1"
  CXXFLAGS="$CXXFLAGS -DUTS_RECURSIVE_FOR=1"
  cores=1
  nodes=1
fi

make clean

CXXFLAGS=$CXXFLAGS make uts.out

# for binomial trees (not working)
# export MADM_COMM_ALLOCATOR_INIT_SIZE=$((4 * 1024 * 1024))
# export MADM_FUTURE_POOL_BUF_SIZE=$((2 * 1024 * 1024))
# export MADM_STACK_SIZE=$((64 * 1024 * 1024))
# export MADM_TASKQ_CAPACITY=$((64 * 1024))

# for i in $(seq 1 $nodes); do
for i in $nodes; do
# for i in 1 2 4 8 16; do
  echo
  echo "## Running on $i nodes"
  echo

  dnp_mpirun $((cores * i)) $cores ./uts.out $tree -i $n_repeats
  # dnp_mpirun $((cores * i)) $cores ${HOME_DIR}/massivethreads-dm/uth/examples/uts/uts $tree
done
