#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..
source run_jobs/common.bash

case $MACHINE_NAME in
  ito)
    # N=1024
    # N=2048
    # N=4096
    # N=8192
    N=16384
    # N=32768
    # N=65536
    # N=131072
    # N=262144
    # N=524288
    # N=1048576
    # N=2097152
    # N=4194304
    # N=8388608
    # N=16777216
    # N=33554432

    M=10000 # 10 us
    # M=100000 # 100 us
    ;;
  wisteria-o)
    # N=1024
    # N=2048
    # N=4096
    # N=8192
    # N=16384
    # N=32768
    # N=65536
    # N=131072
    N=262144
    # N=524288
    # N=1048576
    # N=2097152
    # N=4194304
    # N=8388608
    # N=16777216
    # N=33554432

    M=10000 # 10 us
    # M=20000 # 20 us
    # M=50000 # 50 us
    # M=100000 # 100 us
    ;;
  *)
    # N=1024
    # N=2048
    # N=4096
    # N=8192
    # N=16384
    N=32768
    # N=65536

    M=10000 # 10 us
    # M=100000 # 100 us
    ;;
esac

compute_pattern=pfor
# compute_pattern=rrm
# compute_pattern=lcs
# compute_pattern=mm

mpi_bench=0

debug=0

prof=0
# prof=1

# n_blocks=1
n_blocks=5

# n_warmup=0
# n_warmup=100
# n_warmup=500
n_warmup=1000

CXXFLAGS=""
# CXXFLAGS="$CXXFLAGS -O0 -g"
# CXXFLAGS="$CXXFLAGS -DDNPPATTERN_SERIAL=1"

if [[ $prof == 1 ]]; then
  CXXFLAGS="$CXXFLAGS -DDNPLOGGER_ENABLE=1"

  output_trace=1

  n_repeats=1
  # n_repeats=3
else
  output_trace=0

  # n_repeats=3
  # n_repeats=5
  n_repeats=10
  # n_repeats=20
  # n_repeats=100
fi

# CXXFLAGS="$CXXFLAGS -DDNPLOGGER_PRINT_STAT_PER_RANK=1"
CXXFLAGS="$CXXFLAGS -DDNPLOGGER_OUTPUT_TRACE=$output_trace"
CXXFLAGS="$CXXFLAGS -DDNP_LOGGER_PRINT_STAT=1"

if [[ $debug == 1 ]]; then
  CXXFLAGS="$CXXFLAGS -O0 -g"
fi

make clean

if [[ $mpi_bench == 1 ]]; then
  CXXFLAGS=$CXXFLAGS make sched_bench_mpi.out
else
  CXXFLAGS=$CXXFLAGS make sched_bench.out
fi

# export MADM_COMM_ALLOCATOR_INIT_SIZE=$((4 * 1024 * 1024))
# export MADM_FUTURE_POOL_BUF_SIZE=$((2 * 1024 * 1024))
# export MADM_FUTURE_POOL_BUF_SIZE=$((1 * 1024 * 1024))
# export MADM_TASKQ_CAPACITY=65536

export MADM_FUTURE_POOL_LOCAL_BUF_SIZE=1
export MADM_SUSPENDED_LOCAL_BUF_SIZE=1
# export MADM_FUTURE_POOL_LOCAL_BUF_SIZE=10
# export MADM_SUSPENDED_LOCAL_BUF_SIZE=10

export MADM_STACK_FRAME_SIZE=$((16 * 1024))
# export MADM_STACK_FRAME_SIZE=$((32 * 1024))

# for child_stealing_ult
# export MADM_STACK_FRAME_SIZE=$((32 * 1024))
# export MADM_COMM_ALLOCATOR_INIT_SIZE=$((4 * 1024 * 1024))
# export MADM_FUTURE_POOL_BUF_SIZE=$((2 * 1024 * 1024))

# export MADM_LOGGER_INITIAL_SIZE=$((256 * 1024 * 1024))
# export MADM_LOGGER_PRINT_STAT_PER_RANK=1
export MADM_LOGGER_OUTPUT_TRACE=$output_trace

# for i in $(seq 1 $nodes); do
for i in $nodes; do
# for i in 1 2 4 8 16; do
  echo
  echo "## Running on $i nodes"
  echo

  case $compute_pattern in
    pfor)
      W=$((N * M * n_blocks))
      S=$((M * n_blocks))
      ;;
    rrm)
      W=$((n_blocks * M * N * $(log2 N) + M * N))
      S=$((n_blocks * M * $(log2 N) + M))
      ;;
    lcs)
      W=$((M * N * N))
      S=$((M * (3 ** $(log2 N))))
      ;;
    mm)
      W=$((M * N * N * N))
      S=$((M * N))
      ;;
  esac

  I=$((S > W / cores / i ? S : W / cores / i))
  echo
  echo "### N = $N, M = $M"
  echo "    Work:  $(LC_ALL=en_US.UTF-8 printf "%'20d" $W) ns"
  echo "    Span:  $(LC_ALL=en_US.UTF-8 printf "%'20d" $S) ns"
  echo "    Ideal: $(LC_ALL=en_US.UTF-8 printf "%'20d" $I) ns"
  echo

  if [[ $debug == 0 ]]; then
    if [[ $mpi_bench == 1 ]]; then
      dnp_mpirun $((cores * i)) $cores ./sched_bench_mpi.out -n $N -m $M -b $n_blocks -r $n_repeats -f $CPU_SIMD_FREQ -w $n_warmup
    else
      dnp_mpirun $((cores * i)) $cores ./sched_bench.out -c $compute_pattern -n $N -m $M -b $n_blocks -r $n_repeats -f $CPU_SIMD_FREQ -w $n_warmup
    fi
  else
    # export USE_VIM=false
    # export TARGET_RANKS="0 1"
    dnp_mpirun $((cores * i)) $cores ./scripts/debugmpi.bash ./sched_bench.out -c $compute_pattern -n $N -m $M -b $n_blocks -r $n_repeats -f $CPU_SIMD_FREQ -w 0
    # tmpi $((cores * i)) ${HOME_DIR}/opt/massivethreads-dm/bin/madm_disable_aslr gdb --arg ./sched_bench.out -c $compute_pattern -n $N -m $M -b $n_blocks -r $n_repeats -f $CPU_SIMD_FREQ -w 0
  fi
  # cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq

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
