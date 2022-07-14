#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/

# load gcc 11.2.0
export PATH=$HOME/opt/gcc/11.2.0/bin:$PATH
export LD_LIBRARY_PATH=$HOME/opt/gcc/11.2.0/lib64:$LD_LIBRARY_PATH

# SAWS_OSH_IMPL=${SAWS_OSH_IMPL:-sos}
SAWS_OSH_IMPL=${SAWS_OSH_IMPL:-omp5}

case $SAWS_OSH_IMPL in
  sos)
    # load Sandia OpenSHMEM (SOS)
    export PATH=$HOME/opt/SOS/bin:$PATH
    export LD_LIBRARY_PATH=$HOME/opt/SOS/lib:$LD_LIBRARY_PATH
    ;;
  omp5)
    # load Open MPI v5
    export PATH=$HOME/opt/openmpi5/bin:$PATH
    export LD_LIBRARY_PATH=$HOME/opt/openmpi5/lib:$LD_LIBRARY_PATH
    ;;
esac

cd examples/uts

which oshcc
oshrun --version

make -j
