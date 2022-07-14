#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/examples/uts

source sample_trees.sh

# SAWS_OSH_IMPL=${SAWS_OSH_IMPL:-sos}
SAWS_OSH_IMPL=${SAWS_OSH_IMPL:-omp5}

# mpirun command is needed for oshrun anyway
# load Open MPI v5
export PATH=$HOME/opt/openmpi5/bin:$PATH
export LD_LIBRARY_PATH=$HOME/opt/openmpi5/lib:$LD_LIBRARY_PATH

case $SAWS_OSH_IMPL in
  sos)
    # load Sandia OpenSHMEM (SOS)
    export PATH=$HOME/opt/SOS/bin:$PATH
    export LD_LIBRARY_PATH=$HOME/opt/SOS/lib:$LD_LIBRARY_PATH
    ;;
  omp5)
    # already loaded
    ;;
esac

UTS_TREE=${UTS_TREE:-T1L}
# UTS_TREE=${UTS_TREE:-T1XL}
# UTS_TREE=${UTS_TREE:-T1XXL}
# UTS_TREE=${UTS_TREE:-T1WL}

NODES=${NODES:-1}
# NODES=${NODES:-4}
# NODES=${NODES:-16}
# NODES=${NODES:-64}
# NODES=${NODES:-128}

# CORES=${CORES:-1}
CORES=${CORES:-36}

REPEATS=${REPEATS:-10}
# REPEATS=${REPEATS:-100}

WARMUPS=${WARMUPS:-0}
# WARMUPS=${WARMUPS:-10}
# WARMUPS=${WARMUPS:-100}

OUT_DIR=${OUT_DIR:-./out}

mkdir -p $OUT_DIR

which oshrun
oshrun --version

echo
echo "### Node = $NODES, Core = $CORES, Tree = $UTS_TREE"
echo

out_file=${OUT_DIR}/node_${NODES}_core_${CORES}_tree_${UTS_TREE}.txt
oshrun \
  --mca plm_rsh_agent pjrsh \
  --hostfile $PJM_O_NODEINF \
  --bind-to core \
  -n $((NODES * CORES)) \
  -N $CORES \
  ./uts-scioto ${!UTS_TREE} -i $((REPEATS + WARMUPS)) -Q H | tee $out_file
