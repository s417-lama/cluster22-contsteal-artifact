#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/

MACHINE_NAME=${MACHINE_NAME:-ito}

case $MACHINE_NAME in
  ito)
    # load gcc 11.2.0
    export PATH=$HOME/opt/gcc/11.2.0/bin:$PATH
    export LD_LIBRARY_PATH=$HOME/opt/gcc/11.2.0/lib64:$LD_LIBRARY_PATH

    # load Open MPI 5
    export PATH=$HOME/opt/openmpi5/bin:$PATH
    export LD_LIBRARY_PATH=$HOME/opt/openmpi5/lib:$LD_LIBRARY_PATH

    module load cmake/3.18.2

    export CXXFLAGS="-O3 -march=native"
    ;;
esac

which mpicxx
mpicxx --version

./build LIBS mpi-linux-x86_64 mpicxx --with-production -j32

cd examples/charm++/state_space_searchengine/UnbalancedTreeSearch_SE

make OPTS="$CXXFLAGS" -j8
