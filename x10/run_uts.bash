#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/GLB-UTS

MACHINE_NAME=${MACHINE_NAME:-ito}

source sample_trees.sh

case $MACHINE_NAME in
  ito)
    # load gcc 11.2.0
    export PATH=$HOME/opt/gcc/11.2.0/bin:$PATH
    export LD_LIBRARY_PATH=$HOME/opt/gcc/11.2.0/lib64:$LD_LIBRARY_PATH

    # load Open MPI 5
    export PATH=$HOME/opt/openmpi5/bin:$PATH
    export LD_LIBRARY_PATH=$HOME/opt/openmpi5/lib:$LD_LIBRARY_PATH

    MAX_CORES=36
    ;;
esac

UTS_TREE=${UTS_TREE:-T1L}
# UTS_TREE=${UTS_TREE:-T1XXL}
# UTS_TREE=${UTS_TREE:-T1WL}

NODES=${NODES:-1}
# NODES=${NODES:-4}
# NODES=${NODES:-16}
# NODES=${NODES:-64}
# NODES=${NODES:-128}

# CORES=${CORES:-1}
CORES=${CORES:-$MAX_CORES}

REPEATS=${REPEATS:-10}
# REPEATS=${REPEATS:-100}

WARMUPS=${WARMUPS:-0}
# WARMUPS=${WARMUPS:-10}
# WARMUPS=${WARMUPS:-100}

OUT_DIR=${OUT_DIR:-./out}

mkdir -p $OUT_DIR

which mpirun
mpirun --version

echo
echo "### Node = $NODES, Core = $CORES, Tree = $UTS_TREE"
echo

case $MACHINE_NAME in
  ito)
    out_file=${OUT_DIR}/node_${NODES}_core_${CORES}_tree_${UTS_TREE}.txt
    mpirun \
      --mca plm_rsh_agent pjrsh \
      --hostfile $PJM_O_NODEINF \
      --bind-to core \
      --mca osc_ucx_acc_single_intrinsic true \
      -x X10_NTHREADS=1 \
      -n $((NODES * CORES)) \
      -N $CORES \
      ./myuts-native ${!UTS_TREE} -i $((REPEATS + WARMUPS)) | tee $out_file
    ;;
esac
