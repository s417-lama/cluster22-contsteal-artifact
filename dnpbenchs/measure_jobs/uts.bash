#!/bin/bash

# UTS_TREE=${UTS_TREE:-T1L}
# UTS_TREE=${UTS_TREE:-T1XL}
# UTS_TREE=${UTS_TREE:-T1XXL}
UTS_TREE=${UTS_TREE:-T1WL}

# MEASURE_CONFIG=${MEASURE_CONFIG:-serial}
# MEASURE_CONFIG=${MEASURE_CONFIG:-onecore}
MEASURE_CONFIG=${MEASURE_CONFIG:-allcore}

DNP_PREFIX=uts_${UTS_TREE}_${MEASURE_CONFIG}

# DNP_MADM_BUILD=${DNP_MADM_BUILD:-waitq}
# DNP_MADM_BUILD=${DNP_MADM_BUILD:-waitq_gc}
# DNP_MADM_BUILD=${DNP_MADM_BUILD:-greedy}
DNP_MADM_BUILD=${DNP_MADM_BUILD:-greedy_gc}

DNP_MAX_NODES=${DNP_MAX_NODES:-1}
# DNP_MAX_NODES=${DNP_MAX_NODES:-4}
# DNP_MAX_NODES=${DNP_MAX_NODES:-16}
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

N_WARMUP=${N_WARMUP:-10}
# N_WARMUP=${N_WARMUP:-100}
# N_WARMUP=${N_WARMUP:-1000}

dnp_run() {
  dnp_run_common

  source uts/sample_trees.sh

  CXXFLAGS=""

  n_nodes=$(echo $DNP_MAX_NODES | cut -f 1 -d ":" | sed 's/x/*/g' | bc)

  case $MEASURE_CONFIG in
    serial)
      n_cores=1
      CXXFLAGS="$CXXFLAGS -DUTS_RUN_SEQ=1"
      CXXFLAGS="$CXXFLAGS -DUTS_RECURSIVE_FOR=1"
      ;;
    onecore)
      n_cores=1
      ;;
    allcore)
      n_cores=$DNP_CORES
      ;;
  esac

  make clean
  CXXFLAGS=$CXXFLAGS make uts.out

  mkdir -p $OUT_DIR

  for i in $(seq 1 $REPEAT_OUTSIDE); do
    echo
    echo "### Node = $n_nodes, Tree = $UTS_TREE"
    echo
    out_file=${OUT_DIR}/node_${n_nodes}_tree_${UTS_TREE}_i_${i}.txt
    dnp_mpirun $((n_nodes * n_cores)) $n_cores $out_file ./uts.out ${!UTS_TREE} -i $((REPEAT_INSIDE + N_WARMUP))
  done

  ./a2sql/bin/txt2sql result.db \
    --drop --table dnpbenchs \
    -e '# of processes: *(?P<np>\d+)' \
    -r '\[(?P<i>\d+)\] *(?P<time>\d+) *ns *(?P<throughput>[-+]?(\d+(\.\d*)?|\.\d+)([eE][-+]?\d+)?) *Gnodes/s *\( *nodes: *(?P<nodes>\d+) *depth: *(?P<depth>\d+) *leaves: *(?P<leaves>\d+) *\)' \
    ${OUT_DIR}/*.txt
}
