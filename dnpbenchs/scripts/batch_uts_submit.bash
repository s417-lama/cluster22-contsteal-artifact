#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..

MACHINE_NAME=$1
JOB_FILE=./measure_jobs/uts.bash

export DNP_MADM_BUILD=greedy_gc

export REPEAT_OUTSIDE=1

export REPEAT_INSIDE=3
# export REPEAT_INSIDE=100

case $MACHINE_NAME in
  ito)
    # -------------------- serial --------------------

    # T1L
    (
      export MEASURE_CONFIG=serial
      export DNP_MAX_NODES=1
      export UTS_TREE=T1L
      export N_WARMUP=10
      export EXEC_TIME=1:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 1 core --------------------

    # T1L
    (
      export MEASURE_CONFIG=onecore
      export DNP_MAX_NODES=1
      export UTS_TREE=T1L
      export N_WARMUP=10
      export EXEC_TIME=1:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 1 node --------------------

    # T1L
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=1
      export UTS_TREE=T1L
      export N_WARMUP=10
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 4 nodes --------------------

    # T1L
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=4
      export UTS_TREE=T1L
      export N_WARMUP=100
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1XXL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=4
      export UTS_TREE=T1XXL
      export N_WARMUP=10
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 16 nodes --------------------

    # T1L
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=16
      export UTS_TREE=T1L
      export N_WARMUP=1000
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1XXL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=16
      export UTS_TREE=T1XXL
      export N_WARMUP=100
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 64 nodes --------------------

    # T1L
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=64
      export UTS_TREE=T1L
      export N_WARMUP=10000
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1XXL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=64
      export UTS_TREE=T1XXL
      export N_WARMUP=1000
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1WL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=64
      export UTS_TREE=T1WL
      export N_WARMUP=100
      export EXEC_TIME=2:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 256 nodes --------------------

    # T1L
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=256
      export UTS_TREE=T1L
      export N_WARMUP=100000
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1XXL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=256
      export UTS_TREE=T1XXL
      export N_WARMUP=10000
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1WL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=256
      export UTS_TREE=T1WL
      export N_WARMUP=100
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    wait
    ;;
  wisteria-o)
    # -------------------- serial --------------------

    # T1L
    (
      export MEASURE_CONFIG=serial
      export DNP_MAX_NODES=1
      export UTS_TREE=T1L
      export N_WARMUP=10
      export EXEC_TIME=4:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 1 core --------------------

    # T1L
    (
      export MEASURE_CONFIG=onecore
      export DNP_MAX_NODES=1
      export UTS_TREE=T1L
      export N_WARMUP=10
      export EXEC_TIME=4:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 1 node --------------------

    # T1L
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=1
      export UTS_TREE=T1L
      export N_WARMUP=10
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 6 nodes --------------------

    # T1L
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=2x3x1:mesh
      export UTS_TREE=T1L
      export N_WARMUP=100
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1XXL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=2x3x1:mesh
      export UTS_TREE=T1XXL
      export N_WARMUP=10
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 36 nodes --------------------

    # T1L
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=3x4x3:mesh
      export UTS_TREE=T1L
      export N_WARMUP=1000
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1XXL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=3x4x3:mesh
      export UTS_TREE=T1XXL
      export N_WARMUP=100
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 144 nodes --------------------

    # T1L
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=6x6x4:mesh
      export UTS_TREE=T1L
      export N_WARMUP=10000
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1XXL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=6x6x4:mesh
      export UTS_TREE=T1XXL
      export N_WARMUP=1000
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1WL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=6x6x4:mesh
      export UTS_TREE=T1WL
      export N_WARMUP=10
      export EXEC_TIME=2:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 576 nodes --------------------

    # T1L
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=8x9x8:mesh
      export UTS_TREE=T1L
      export N_WARMUP=100000
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1XXL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=8x9x8:mesh
      export UTS_TREE=T1XXL
      export N_WARMUP=1000
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1WL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=8x9x8:mesh
      export UTS_TREE=T1WL
      export N_WARMUP=100
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 2304 nodes --------------------

    # T1L
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=12x16x12:mesh
      export UTS_TREE=T1L
      export N_WARMUP=100000
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1XXL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=12x16x12:mesh
      export UTS_TREE=T1XXL
      export N_WARMUP=10000
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # T1WL
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=12x16x12:mesh
      export UTS_TREE=T1WL
      export N_WARMUP=100
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    wait
    ;;
  *)
    echo "Unknown machine name '$MACHINE_NAME'"
    exit 1
    ;;
esac
