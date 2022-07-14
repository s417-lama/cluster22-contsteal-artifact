#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..
source run_jobs/common.bash

case $MACHINE_NAME in
  ito)
    # N=2048
    # N=4096
    # N=8192
    # N=16384
    # N=32768
    # N=65536
    N=131072
    # N=262144
    # N=524288
    # N=1048576
    # N=2097152
    # N=4194304
    ;;
  wisteria-o)
    # N=2048
    # N=4096
    # N=8192
    # N=16384
    # N=32768
    # N=65536
    # N=131072
    # N=262144
    # N=524288
    N=1048576
    # N=2097152
    # N=4194304
    ;;
  *)
    # N=1024
    N=2048
    # N=16384
    ;;
esac

# nodes=1
# cores=1

serial=0

debug=0

prof=0

# cutoff=32
# cutoff=64
# cutoff=128
# cutoff=256
cutoff=512
# cutoff=1024

check=0

CXXFLAGS=""
# CXXFLAGS="$CXXFLAGS -O0 -g"
CXXFLAGS="$CXXFLAGS -DDNP_LCS_CUTOFF=$cutoff"

if [[ $prof == 1 ]]; then
  CXXFLAGS="$CXXFLAGS -DDNPLOGGER_ENABLE=1"

  # n_repeats=1
  # n_repeats=2
  n_repeats=3
else
  # n_repeats=1
  n_repeats=10
  # n_repeats=100
  # n_repeats=1000
fi

# CXXFLAGS="$CXXFLAGS -DDNPLOGGER_PRINT_STAT_PER_RANK=1"
CXXFLAGS="$CXXFLAGS -DDNP_LOGGER_PRINT_STAT=1"

if [[ $debug == 1 ]]; then
  CXXFLAGS="$CXXFLAGS -O0 -g"
fi

make clean

CXXFLAGS=$CXXFLAGS make lcs.out

# for child_stealing_ult
export MADM_STACK_FRAME_SIZE=$((32 * 1024))

# export MADM_COMM_ALLOC_BITS=23 # 8 MB / process
# export MADM_COMM_ALLOCATOR_INIT_SIZE=$((6 * 1024 * 1024))
# export MADM_FUTURE_POOL_BUF_SIZE=$((3 * 1024 * 1024))

# export MADM_COMM_ALLOC_BITS=24 # 16 MB / process
# export MADM_COMM_ALLOCATOR_INIT_SIZE=$((14 * 1024 * 1024))
# export MADM_FUTURE_POOL_BUF_SIZE=$((7 * 1024 * 1024))

# export MADM_COMM_ALLOC_BITS=25 # 32 MB / process
# export MADM_COMM_ALLOCATOR_INIT_SIZE=$((30 * 1024 * 1024))
# export MADM_FUTURE_POOL_BUF_SIZE=$((15 * 1024 * 1024))

# export MADM_COMM_ALLOC_BITS=26 # 64 MB / process
# export MADM_COMM_ALLOCATOR_INIT_SIZE=$((60 * 1024 * 1024))
# export MADM_FUTURE_POOL_BUF_SIZE=$((30 * 1024 * 1024))

# export MADM_COMM_ALLOC_BITS=27 # 128 MB / process
# export MADM_COMM_ALLOCATOR_INIT_SIZE=$((120 * 1024 * 1024))
# export MADM_FUTURE_POOL_BUF_SIZE=$((60 * 1024 * 1024))

export MADM_COMM_ALLOC_BITS=28 # 256 MB / process
export MADM_COMM_ALLOCATOR_INIT_SIZE=$((240 * 1024 * 1024))
export MADM_FUTURE_POOL_BUF_SIZE=$((120 * 1024 * 1024))

# for i in $(seq 1 $nodes); do
for i in $nodes; do
# for i in 1 2 4 8 16; do
  echo
  echo "## Running on $i nodes"
  echo

  if [[ $debug == 0 ]]; then
    dnp_mpirun $((cores * i)) $cores ./lcs.out -n $N -r $n_repeats -c $check -s $serial -w 0
  else
    # export USE_VIM=false
    # export TARGET_RANKS="0 1"
    dnp_mpirun $((cores * i)) $cores ./scripts/debugmpi.bash ./lcs.out -n $N -r $n_repeats -c 0 -w 0
    # tmpi $((cores * i)) ${HOME_DIR}/opt/massivethreads-dm/bin/madm_disable_aslr gdb --arg ./lcs.out -n $N -r $n_repeats -c 0 -w 0
  fi

  echo > mlog.ignore
  if [[ -f dnp_log_0.ignore ]]; then
    cat dnp_log_*.ignore > dnp_log.ignore
    rm dnp_log_*.ignore
    cat dnp_log.ignore >> mlog.ignore
  fi
  if [[ -f madm_log_0.ignore ]]; then
    cat madm_log_*.ignore > madm_log.ignore
    rm madm_log_*.ignore
    cat madm_log.ignore >> mlog.ignore
  fi
  ls -lh mlog.ignore
done
