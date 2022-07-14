#!/bin/bash

# INPUT_N=${INPUT_N:-65536}
# INPUT_N=${INPUT_N:-131072}
# INPUT_N=${INPUT_N:-262144}
# INPUT_N=${INPUT_N:-524288}
# INPUT_N=${INPUT_N:-1048576}
# INPUT_N=${INPUT_N:-2097152}
# INPUT_N=${INPUT_N:-4194304}
# INPUT_N=${INPUT_N:-8388608}
INPUT_N=${INPUT_N:-16777216}
# INPUT_N=${INPUT_N:-33554432}
# INPUT_N=${INPUT_N:-67108864}
# INPUT_N=${INPUT_N:-134217728}

CUTOFF_M=${CUTOFF_M:-256}
# CUTOFF_M=${CUTOFF_M:-512}
# CUTOFF_M=${CUTOFF_M:-1024}

# MEASURE_CONFIG=${MEASURE_CONFIG:-serial}
# MEASURE_CONFIG=${MEASURE_CONFIG:-onecore}
MEASURE_CONFIG=${MEASURE_CONFIG:-allcore}

DNP_PREFIX=lcs_${INPUT_N}_${CUTOFF_M}_${MEASURE_CONFIG}

DNP_MADM_BUILD=${DNP_MADM_BUILD:-future_multi}

# DNP_MAX_NODES=${DNP_MAX_NODES:-1}
# DNP_MAX_NODES=${DNP_MAX_NODES:-4}
# DNP_MAX_NODES=${DNP_MAX_NODES:-16}
# DNP_MAX_NODES=${DNP_MAX_NODES:-64}
# DNP_MAX_NODES=${DNP_MAX_NODES:-256}

# DNP_MAX_NODES=${DNP_MAX_NODES:-1}
# DNP_MAX_NODES=${DNP_MAX_NODES:-2x3x1:mesh}
# DNP_MAX_NODES=${DNP_MAX_NODES:-3x4x3:mesh}
# DNP_MAX_NODES=${DNP_MAX_NODES:-6x6x4:mesh}
DNP_MAX_NODES=${DNP_MAX_NODES:-8x9x8:mesh}
# DNP_MAX_NODES=${DNP_MAX_NODES:-12x16x12:mesh}

source measure_jobs/common.bash

OUT_DIR=${OUT_DIR:-./out}

REPEAT_OUTSIDE=${REPEAT_OUTSIDE:-1}
REPEAT_INSIDE=${REPEAT_INSIDE:-10}
# REPEAT_INSIDE=${REPEAT_INSIDE:-100}

N_WARMUP=${N_WARMUP:-1}
# N_WARMUP=${N_WARMUP:-10}
# N_WARMUP=${N_WARMUP:-100}
# N_WARMUP=${N_WARMUP:-1000}

PREALLOC_SIZE=${PREALLOC_SIZE:-24}
# PREALLOC_SIZE=${PREALLOC_SIZE:-25}
# PREALLOC_SIZE=${PREALLOC_SIZE:-26}

dnp_run() {
  dnp_run_common

  CXXFLAGS=""
  CXXFLAGS="$CXXFLAGS -DDNPLOGGER_ENABLE=0"
  CXXFLAGS="$CXXFLAGS -DDNPLOGGER_OUTPUT_TRACE=0"
  CXXFLAGS="$CXXFLAGS -DDNP_LCS_CUTOFF=$CUTOFF_M"

  n_nodes=$(echo $DNP_MAX_NODES | cut -f 1 -d ":" | sed 's/x/*/g' | bc)

  case $MEASURE_CONFIG in
    serial)
      n_cores=1
      serial=1
      ;;
    onecore)
      n_cores=1
      serial=0
      ;;
    allcore)
      n_cores=$DNP_CORES
      serial=0
      ;;
  esac

  make clean
  CXXFLAGS=$CXXFLAGS make lcs.out

  export MADM_COMM_ALLOC_BITS=$PREALLOC_SIZE
  export MADM_COMM_ALLOCATOR_INIT_SIZE=$((2 ** PREALLOC_SIZE - 1024 * 1024))
  export MADM_FUTURE_POOL_BUF_SIZE=$((2 ** (PREALLOC_SIZE - 1)))

  if [[ $DNP_MADM_BUILD == child_stealing_ult* ]]; then
    export MADM_STACK_FRAME_SIZE=$((32 * 1024))
  fi

  mkdir -p $OUT_DIR

  for i in $(seq 1 $REPEAT_OUTSIDE); do
    echo
    echo "### Node = $n_nodes, N = $INPUT_N, M = $CUTOFF_M"
    echo
    out_file=${OUT_DIR}/node_${n_nodes}_n_${INPUT_N}_m_${CUTOFF_M}_i_${i}.txt
    dnp_mpirun $((n_nodes * n_cores)) $n_cores $out_file ./lcs.out -n $INPUT_N -s $serial -c 0 -r $REPEAT_INSIDE -w $N_WARMUP
  done

  ./a2sql/bin/txt2sql result.db \
    --drop --table dnpbenchs \
    -e '# of processes: *(?P<np>\d+)' \
    -e 'N \(Input size\): *(?P<n>\d+)' \
    -e '# of repeats: *(?P<rep>\d+)' \
    -e '# of warmup runs: *(?P<warmup>\d+)' \
    -e 'Cutoff: *(?P<cutoff>\d+)' \
    -r '\[(?P<i>\d+)\] *(?P<time>\d+) *ns' \
    ${OUT_DIR}/*.txt
}
