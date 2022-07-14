#!/bin/bash

# MEASURE_PATTERN=${MEASURE_PATTERN:-strong}
MEASURE_PATTERN=${MEASURE_PATTERN:-size_n}
# MEASURE_PATTERN=${MEASURE_PATTERN:-size_nm}

MEASURE_CONFIG=${MEASURE_CONFIG:-no_buf}
# MEASURE_CONFIG=${MEASURE_CONFIG:-buf_10}

DNP_PREFIX=sched_rrm_${MEASURE_PATTERN}_${MEASURE_CONFIG}

# DNP_MADM_BUILD=${DNP_MADM_BUILD:-waitq}
# DNP_MADM_BUILD=${DNP_MADM_BUILD:-waitq_gc}
# DNP_MADM_BUILD=${DNP_MADM_BUILD:-greedy}
# DNP_MADM_BUILD=${DNP_MADM_BUILD:-greedy_gc}
# DNP_MADM_BUILD=${DNP_MADM_BUILD:-child_stealing}
# DNP_MADM_BUILD=${DNP_MADM_BUILD:-child_stealing_ult}
# DNP_MADM_BUILD=${DNP_MADM_BUILD:-waitq_prof}
# DNP_MADM_BUILD=${DNP_MADM_BUILD:-waitq_gc_prof}
# DNP_MADM_BUILD=${DNP_MADM_BUILD:-greedy_gc_prof}
# DNP_MADM_BUILD=${DNP_MADM_BUILD:-child_stealing_prof}
DNP_MADM_BUILD=${DNP_MADM_BUILD:-child_stealing_ult_prof}

# DNP_MAX_NODES=${DNP_MAX_NODES:-1}
# DNP_MAX_NODES=${DNP_MAX_NODES:-4}
DNP_MAX_NODES=${DNP_MAX_NODES:-16}
# DNP_MAX_NODES=${DNP_MAX_NODES:-64}
# DNP_MAX_NODES=${DNP_MAX_NODES:-256}

# DNP_MAX_NODES=${DNP_MAX_NODES:-1}
# DNP_MAX_NODES=${DNP_MAX_NODES:-2x3x1:mesh}
# DNP_MAX_NODES=${DNP_MAX_NODES:-3x4x3:mesh}
# DNP_MAX_NODES=${DNP_MAX_NODES:-6x6x4:mesh}
# DNP_MAX_NODES=${DNP_MAX_NODES:-8x9x8:mesh}
# DNP_MAX_NODES=${DNP_MAX_NODES:-12x16x12:mesh}

source measure_jobs/common.bash

OUT_DIR=${OUT_DIR:-./out}

REPEAT_OUTSIDE=${REPEAT_OUTSIDE:-1}
REPEAT_INSIDE=${REPEAT_INSIDE:-100}

# N_WARMUP=${N_WARMUP:-1000}
N_WARMUP=${N_WARMUP:-2000}

# N_BLOCKS=${N_BLOCKS:-3}
N_BLOCKS=${N_BLOCKS:-5}

dnp_run() {
  dnp_run_common

  n_nodes=$(echo $DNP_MAX_NODES | cut -f 1 -d ":" | sed 's/x/*/g' | bc)

  case $MEASURE_PATTERN in
    strong)
      NODEs=(1 2 4 8 16)
      M=10000
      NMs=(8192,$M 16384,$M 32768,$M 65536,$M 131072,$M 262144,$M 524288,$M 1048576,$M)
      ;;
    size_n)
      NODEs=($n_nodes)
      M=10000
      # NMs=($(for n in $(seq 0 6); do echo $((4096 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done))
      NMs=($(for n in $(seq 0 7); do echo $((2048 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done))
      ;;
    size_nm)
      NODEs=($n_nodes)
      NMs=($(
        M=1000
        # for n in $(seq 0 6); do echo $((32768 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done
        for n in $(seq 0 7); do echo $((16384 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done
        M=2000
        # for n in $(seq 0 6); do echo $((16384 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done
        for n in $(seq 0 7); do echo $((8192 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done
        M=5000
        # for n in $(seq 0 6); do echo $((8192 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done
        for n in $(seq 0 7); do echo $((4096 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done
        M=10000
        # for n in $(seq 0 6); do echo $((4096 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done
        for n in $(seq 0 7); do echo $((2048 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done
        M=100000
        # for n in $(seq 0 6); do echo $((512 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done
        for n in $(seq 0 7); do echo $((256 * (2 ** (n + $(log2 $((n_nodes+1))) - 1)))),$M; done
      ))
      ;;
  esac

  CXXFLAGS=""
  CXXFLAGS="$CXXFLAGS -DDNPLOGGER_ENABLE=0"
  CXXFLAGS="$CXXFLAGS -DDNPLOGGER_OUTPUT_TRACE=0"

  if [[ $DNP_MADM_BUILD == *prof ]]; then
    CXXFLAGS="$CXXFLAGS -DDNP_LOGGER_PRINT_STAT=1"
  fi

  make clean
  CXXFLAGS=$CXXFLAGS make sched_bench.out

  # export MADM_COMM_ALLOCATOR_INIT_SIZE=$((32 * 1024 * 1024))
  # export MADM_FUTURE_POOL_BUF_SIZE=$((2 * 1024 * 1024))

  if [[ $DNP_MADM_BUILD == child_stealing_ult* ]]; then
    export MADM_STACK_FRAME_SIZE=$((32 * 1024))
    export MADM_COMM_ALLOCATOR_INIT_SIZE=$((4 * 1024 * 1024))
    export MADM_FUTURE_POOL_BUF_SIZE=$((2 * 1024 * 1024))
  fi

  case $MEASURE_CONFIG in
    no_buf)
      export MADM_FUTURE_POOL_LOCAL_BUF_SIZE=1
      export MADM_SUSPENDED_LOCAL_BUF_SIZE=1
      ;;
    buf_10)
      export MADM_FUTURE_POOL_LOCAL_BUF_SIZE=10
      export MADM_SUSPENDED_LOCAL_BUF_SIZE=10
      ;;
  esac

  mkdir -p $OUT_DIR

  for node in "${NODEs[@]}"; do
    for nm in "${NMs[@]}"; do
      IFS=',' read n m <<< $nm

      W=$((N_BLOCKS * m * n * $(log2 n) + m * n))
      S=$((N_BLOCKS * m * $(log2 n) + m))
      I=$((S > W / DNP_CORES / node ? S : W / DNP_CORES / node))
      echo
      echo "## NODE = $node ($((node * DNP_CORES)) cores), N = $n, M = $m"
      echo "   Work:  $(LC_ALL=en_US.UTF-8 printf "%'20d" $W) ns"
      echo "   Span:  $(LC_ALL=en_US.UTF-8 printf "%'20d" $S) ns"
      echo "   Ideal: $(LC_ALL=en_US.UTF-8 printf "%'20d" $I) ns"
      echo

      for i in $(seq 1 $REPEAT_OUTSIDE); do
        echo
        echo "### NODE = $node, N = $n, M = $m ($i / $REPEAT_OUTSIDE)"
        echo
        out_file=${OUT_DIR}/node_${node}_n_${n}_m_${m}_i_${i}.txt
        dnp_mpirun $((node * DNP_CORES)) $DNP_CORES $out_file ./sched_bench.out -c rrm -n $n -m $m -b $N_BLOCKS -r $REPEAT_INSIDE -f $CPU_SIMD_FREQ -w $N_WARMUP
      done

    done
  done

  if [[ $DNP_MADM_BUILD == *prof ]]; then
    ./a2sql/bin/txt2sql result.db \
      --drop --table dnpbenchs \
      -e '# of processes: *(?P<np>\d+)' \
      -e 'N \(Input size\): *(?P<n>\d+)' \
      -e 'M \(Leaf execution time in ns\): *(?P<m>\d+)' \
      -e '# of blocks: *(?P<n_blocks>\d+)' \
      -e '\[(?P<i>\d+)\] *(?P<time>\d+) ns *\( *#tasks: *(?P<n_tasks>\d+) *work: *(?P<work>\d+) *ns *span: *(?P<span>\d+) *ns *busy: *(?P<busy>\d+\.\d+) *% *\)' \
      -e 'join_outstanding *:.*\( *(?P<join_acc>\d+) ns.*\) *count: *(?P<join_count>\d+)' \
      -e 'steal_success *:.*\( *(?P<steal_success_acc>\d+) ns.*\) *count: *(?P<steal_success_count>\d+)' \
      -e 'steal_fail *:.*\( *(?P<steal_fail_acc>\d+) ns.*\) *count: *(?P<steal_fail_count>\d+)' \
      -e 'steal_task_copy *:.*\( *(?P<steal_task_copy_acc>\d+) ns.*\)' \
      -r 'steal_task_size *:.*\( *(?P<steal_task_size_acc>\d+) */.*\)' \
      ${OUT_DIR}/*.txt
  else
    ./a2sql/bin/txt2sql result.db \
      --drop --table dnpbenchs \
      -e '# of processes: *(?P<np>\d+)' \
      -e 'N \(Input size\): *(?P<n>\d+)' \
      -e 'M \(Leaf execution time in ns\): *(?P<m>\d+)' \
      -e '# of blocks: *(?P<n_blocks>\d+)' \
      -r '\[(?P<i>\d+)\] *(?P<time>\d+) ns *\( *#tasks: *(?P<n_tasks>\d+) *work: *(?P<work>\d+) *ns *span: *(?P<span>\d+) *ns *busy: *(?P<busy>\d+\.\d+) *% *\)' \
      ${OUT_DIR}/*.txt
  fi
}
