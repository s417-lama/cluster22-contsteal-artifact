#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/..
source scripts/envs.bash

# MADM_BUILDS="waitq waitq_gc greedy_gc child_stealing child_stealing_ult"
MADM_BUILDS="waitq_prof waitq_gc_prof greedy_gc_prof child_stealing_prof child_stealing_ult_prof"

export REPEAT_OUTSIDE=1

export REPEAT_INSIDE=3
# export REPEAT_INSIDE=100

case $MACHINE_NAME in
  ito)
    # export DNP_MAX_NODES=1
    export DNP_MAX_NODES=16
    ;;
  wisteria-o)
    # export DNP_MAX_NODES=1
    export DNP_MAX_NODES=3x4x3:mesh
    ;;
  *)
    export DNP_MAX_NODES=1
    ;;
esac

for madm_build in $MADM_BUILDS; do
  (
    export DNP_MADM_BUILD=$madm_build
    ./scripts/run_measurement.bash $MACHINE_NAME ./measure_jobs/sched_pfor.bash
  )
  (
    export DNP_MADM_BUILD=$madm_build
    ./scripts/run_measurement.bash $MACHINE_NAME ./measure_jobs/sched_rrm.bash
  )
done
