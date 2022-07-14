#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..

MACHINE_NAME=$1
JOB_FILE=./measure_jobs/lcs.bash

export DNP_MADM_BUILD=future_multi
# export DNP_MADM_BUILD=waitq_gc_multi
# export DNP_MADM_BUILD=child_stealing_ult_multi

export REPEAT_OUTSIDE=1

export REPEAT_INSIDE=3
# export REPEAT_INSIDE=10

export CUTOFF_M=512

case $MACHINE_NAME in
  ito)
    # -------------------- 1 core --------------------

    # N=65536
    (
      export MEASURE_CONFIG=onecore
      export DNP_MAX_NODES=1
      export INPUT_N=65536
      export N_WARMUP=1
      export PREALLOC_SIZE=24
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 1 node --------------------

    # N=262144
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=1
      export INPUT_N=262144
      export N_WARMUP=1
      export PREALLOC_SIZE=24
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=1048576
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=1
      export INPUT_N=1048576
      export N_WARMUP=1
      export PREALLOC_SIZE=26
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 4 nodes --------------------

    # N=262144
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=4
      export INPUT_N=262144
      export N_WARMUP=1
      export PREALLOC_SIZE=23
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=1048576
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=4
      export INPUT_N=1048576
      export N_WARMUP=1
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=4194304
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=4
      export INPUT_N=4194304
      export N_WARMUP=1
      export PREALLOC_SIZE=27
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 16 nodes --------------------

    # N=262144
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=16
      export INPUT_N=262144
      export N_WARMUP=1
      export PREALLOC_SIZE=23
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=1048576
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=16
      export INPUT_N=1048576
      export N_WARMUP=1
      export PREALLOC_SIZE=25
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=4194304
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=16
      export INPUT_N=4194304
      export N_WARMUP=1
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=16777216
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=16
      export INPUT_N=16777216
      export N_WARMUP=1
      export PREALLOC_SIZE=27
      export EXEC_TIME=4:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 64 nodes --------------------

    # N=262144
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=64
      export INPUT_N=262144
      export N_WARMUP=10
      export PREALLOC_SIZE=23
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=1048576
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=64
      export INPUT_N=1048576
      export N_WARMUP=10
      export PREALLOC_SIZE=23
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=4194304
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=64
      export INPUT_N=4194304
      export N_WARMUP=1
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=16777216
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=64
      export INPUT_N=16777216
      export N_WARMUP=1
      export PREALLOC_SIZE=26
      export EXEC_TIME=2:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 256 nodes --------------------

    # N=262144
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=256
      export INPUT_N=262144
      export N_WARMUP=100
      export PREALLOC_SIZE=24
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=1048576
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=256
      export INPUT_N=1048576
      export N_WARMUP=10
      export PREALLOC_SIZE=24
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=4194304
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=256
      export INPUT_N=4194304
      export N_WARMUP=10
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=16777216
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=256
      export INPUT_N=16777216
      export N_WARMUP=10
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=67108864
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=256
      export INPUT_N=67108864
      export N_WARMUP=1
      export PREALLOC_SIZE=26
      export EXEC_TIME=3:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    wait
    ;;
  wisteria-o)
    # -------------------- 1 core --------------------

    # N=65536
    (
      export MEASURE_CONFIG=onecore
      export DNP_MAX_NODES=1
      export INPUT_N=65536
      export N_WARMUP=1
      export PREALLOC_SIZE=24
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 1 node --------------------

    # N=262144
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=1
      export INPUT_N=262144
      export N_WARMUP=1
      export PREALLOC_SIZE=24
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=1048576
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=1
      export INPUT_N=1048576
      export N_WARMUP=1
      export PREALLOC_SIZE=26
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 6 nodes --------------------

    # N=262144
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=2x3x1:mesh
      export INPUT_N=262144
      export N_WARMUP=1
      export PREALLOC_SIZE=24
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=1048576
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=2x3x1:mesh
      export INPUT_N=1048576
      export N_WARMUP=1
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=4194304
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=2x3x1:mesh
      export INPUT_N=4194304
      export N_WARMUP=1
      export PREALLOC_SIZE=27
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 36 nodes --------------------

    # N=262144
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=3x4x3:mesh
      export INPUT_N=262144
      export N_WARMUP=1
      export PREALLOC_SIZE=24
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=1048576
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=3x4x3:mesh
      export INPUT_N=1048576
      export N_WARMUP=1
      export PREALLOC_SIZE=25
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=4194304
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=3x4x3:mesh
      export INPUT_N=4194304
      export N_WARMUP=1
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=16777216
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=3x4x3:mesh
      export INPUT_N=16777216
      export N_WARMUP=1
      export PREALLOC_SIZE=27
      export EXEC_TIME=4:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 144 nodes --------------------

    # N=262144
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=6x6x4:mesh
      export INPUT_N=262144
      export N_WARMUP=1
      export PREALLOC_SIZE=24
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=1048576
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=6x6x4:mesh
      export INPUT_N=1048576
      export N_WARMUP=1
      export PREALLOC_SIZE=24
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=4194304
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=6x6x4:mesh
      export INPUT_N=4194304
      export N_WARMUP=1
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=16777216
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=6x6x4:mesh
      export INPUT_N=16777216
      export N_WARMUP=1
      export PREALLOC_SIZE=25
      export EXEC_TIME=2:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 576 nodes --------------------

    # N=262144
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=8x9x8:mesh
      export INPUT_N=262144
      export N_WARMUP=100
      export PREALLOC_SIZE=24
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=1048576
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=8x9x8:mesh
      export INPUT_N=1048576
      export N_WARMUP=10
      export PREALLOC_SIZE=24
      export EXEC_TIME=0:30:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=4194304
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=8x9x8:mesh
      export INPUT_N=4194304
      export N_WARMUP=10
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=16777216
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=8x9x8:mesh
      export INPUT_N=16777216
      export N_WARMUP=10
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=67108864
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=8x9x8:mesh
      export INPUT_N=67108864
      export N_WARMUP=1
      export PREALLOC_SIZE=26
      export EXEC_TIME=3:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # -------------------- 2304 nodes --------------------

    # N=262144
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=12x16x12:mesh
      export INPUT_N=262144
      export N_WARMUP=100
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=1048576
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=12x16x12:mesh
      export INPUT_N=1048576
      export N_WARMUP=10
      export PREALLOC_SIZE=25
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=4194304
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=12x16x12:mesh
      export INPUT_N=4194304
      export N_WARMUP=10
      export PREALLOC_SIZE=26
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=16777216
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=12x16x12:mesh
      export INPUT_N=16777216
      export N_WARMUP=10
      export PREALLOC_SIZE=26
      export EXEC_TIME=1:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    # N=67108864
    (
      export MEASURE_CONFIG=allcore
      export DNP_MAX_NODES=12x16x12:mesh
      export INPUT_N=67108864
      export N_WARMUP=10
      export PREALLOC_SIZE=25
      export EXEC_TIME=2:00:00
      ./scripts/submit.bash $MACHINE_NAME $JOB_FILE
    ) &

    wait
    ;;
  *)
    echo "Unknown machine name '$MACHINE_NAME'"
    exit 1
    ;;
esac
