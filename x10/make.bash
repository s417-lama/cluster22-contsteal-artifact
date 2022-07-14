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

    # load Java 8
    # export JAVA_HOME=/usr/lib/jvm/java-1.8.0-openjdk-1.8.0.302.b08-0.el7_9.x86_64
    export JAVA_HOME=/usr/lib/jvm/java-1.8.0-openjdk-1.8.0.332.b09-1.el7_9.x86_64

    # load Ant 1.10.12
    export ANT_HOME=$HOME/apache-ant-1.10.12
    export PATH=$ANT_HOME/bin:$PATH
    ;;
esac

which mpicxx
mpicxx --version

(
cd x10.dist/
ant -DNO_CHECKS=true -Doptimize=true -DX10RT_MPI=true -Davailable.procs=36 squeakyclean dist
)

(
cd GLB-UTS/
../x10.dist/bin/x10c++ -x10rt mpi -d tmp -STATIC_CHECKS -NO_CHECKS -g -O -O3 -cxx-prearg "-march=native" ./myuts/MyUTS.x10 -o myuts-native
)
